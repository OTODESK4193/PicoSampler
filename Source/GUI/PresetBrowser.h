// ==========================================
// File: PresetBrowser.h
// カテゴリ / 検索 / 保存 / INIT 対応 プリセットブラウザ
//
// プリセットの実体は
//   <UserAppData>/PicoSampler/Presets/<Category>/<Name>.picopreset
// で、APVTS の全パラメータと各スロットのサンプルパスを保持する。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"

class PresetBrowser : public juce::Component
{
public:
    PresetBrowser()
    {
        txtSearch.setTextToShowWhenEmpty("Search presets...", juce::Colours::grey);
        txtSearch.onTextChange = [this] { refreshPresetList(); };
        addAndMakeVisible(txtSearch);

        btnSave.setButtonText("SAVE");
        btnInit.setButtonText("INIT");
        btnClose.setButtonText("CLOSE");

        addAndMakeVisible(btnSave);
        addAndMakeVisible(btnInit);
        addAndMakeVisible(btnClose);

        categoryListModel.owner = this;
        presetListModel.owner   = this;
        categoryListModel.isCategoryList = true;

        lstCategories.setModel(&categoryListModel);
        lstPresets.setModel(&presetListModel);
        lstCategories.setColour(juce::ListBox::backgroundColourId, PicoColors::bgDk);
        lstPresets.setColour(juce::ListBox::backgroundColourId, PicoColors::bgDk);

        addAndMakeVisible(lstCategories);
        addAndMakeVisible(lstPresets);

        btnClose.onClick = [this] { setVisible(false); };
        btnSave.onClick  = [this] { showSaveDialog(); };
        btnInit.onClick  = [this] { showInitDialog(); };
    }

    // ---- ホスト側 (PluginEditor) から差し込むコールバック ----
    std::function<juce::StringArray()>                  getCategories;
    std::function<juce::Array<juce::File>(juce::String)> getPresetsForCategory;
    std::function<void(juce::String, juce::String)>      onSaveRequested;   // (category, name)
    std::function<void(juce::File)>                      onPresetChosen;
    std::function<bool(juce::File)>                      onPresetDeleteRequested;  // 戻り値: 削除成功
    std::function<void()>                                onInitConfirmed;

    void refreshAll()
    {
        categories.clear();
        categories.add(kAllCategories);
        if (getCategories) categories.addArray(getCategories());

        categoryListModel.items = categories;
        lstCategories.updateContent();

        if (selectedCategoryRow >= categories.size()) selectedCategoryRow = 0;
        lstCategories.selectRow(selectedCategoryRow);

        refreshPresetList();
    }

    void visibilityChanged() override
    {
        if (isVisible()) refreshAll();
    }

    void showMessage(const juce::String& title, const juce::String& body)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, title, body, "OK", this);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(PicoColors::bgDk.withAlpha(0.95f));
        g.setColour(PicoColors::panel);
        g.fillRoundedRectangle(getLocalBounds().reduced(20).toFloat(), 8.0f);
        g.setColour(PicoColors::mint);
        g.drawRoundedRectangle(getLocalBounds().reduced(20).toFloat(), 8.0f, 2.0f);

        auto area = getLocalBounds().reduced(30);
        area.removeFromTop(32 + 15);

        g.setColour(PicoColors::textDim);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText("CATEGORY", area.getX(),       area.getY() - 13, 180, 12, juce::Justification::left);
        g.drawText("PRESET",   area.getX() + 195, area.getY() - 13, 200, 12, juce::Justification::left);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(30);
        auto topRow = area.removeFromTop(32);

        txtSearch.setBounds(topRow.removeFromLeft(200));
        btnSave.setBounds(topRow.removeFromRight(70));
        topRow.removeFromRight(10);
        btnInit.setBounds(topRow.removeFromRight(70));
        topRow.removeFromRight(10);
        btnClose.setBounds(topRow.removeFromRight(70));

        area.removeFromTop(15);
        lstCategories.setBounds(area.removeFromLeft(180));
        area.removeFromLeft(15);
        lstPresets.setBounds(area);
    }

private:
    static constexpr const char* kAllCategories = "All";

    void refreshPresetList()
    {
        juce::Array<juce::File> found;
        presetListModel.items.clear();

        const juce::String cat = (selectedCategoryRow > 0 && selectedCategoryRow < categories.size())
                                   ? categories[selectedCategoryRow]
                                   : juce::String();

        if (getPresetsForCategory)
            found = getPresetsForCategory(cat);

        const juce::String filter = txtSearch.getText().trim();

        currentPresets.clear();
        for (const auto& f : found)
        {
            const auto nm = f.getFileNameWithoutExtension();
            if (filter.isEmpty() || nm.containsIgnoreCase(filter))
            {
                currentPresets.add(f);
                presetListModel.items.add(nm);
            }
        }

        lstPresets.updateContent();
        lstPresets.deselectAllRows();
        repaint();
    }

    void categoryRowClicked(int row)
    {
        if (row < 0 || row >= categories.size()) return;
        selectedCategoryRow = row;
        refreshPresetList();
    }

    void presetRowClicked(int row)
    {
        if (row >= 0 && row < currentPresets.size() && onPresetChosen)
            onPresetChosen(currentPresets[row]);
    }

    // ------------------------------------------------------------------
    // プリセット行の右クリックメニュー
    // ------------------------------------------------------------------
    void presetRowRightClicked(int row)
    {
        if (row < 0 || row >= currentPresets.size()) return;

        const juce::File target = currentPresets[row];
        lstPresets.selectRow(row);

        juce::PopupMenu menu;
        menu.addSectionHeader(target.getFileNameWithoutExtension());
        menu.addItem(1, "Load");
        menu.addSeparator();
        menu.addItem(2, "Delete...");

        juce::Component::SafePointer<PresetBrowser> safeThis(this);

        menu.showMenuAsync(juce::PopupMenu::Options()
                               .withTargetComponent(&lstPresets)
                               .withMousePosition(),
            [safeThis, target](int result)
            {
                if (safeThis == nullptr) return;

                if (result == 1)
                {
                    if (safeThis->onPresetChosen) safeThis->onPresetChosen(target);
                }
                else if (result == 2)
                {
                    safeThis->confirmDelete(target);
                }
            });
    }

    void confirmDelete(const juce::File& target)
    {
        juce::Component::SafePointer<PresetBrowser> safeThis(this);

        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon,
            "Delete Preset",
            "Delete the preset \"" + target.getFileNameWithoutExtension() + "\"?\n\n"
            "The preset file will be removed permanently. "
            "The sample files it refers to are not affected.\n\n"
            "This cannot be undone.",
            "Yes",
            "No",
            this,
            juce::ModalCallbackFunction::create([safeThis, target](int result)
            {
                if (result != 1 || safeThis == nullptr) return;

                bool ok = false;
                if (safeThis->onPresetDeleteRequested)
                    ok = safeThis->onPresetDeleteRequested(target);

                if (!ok)
                {
                    safeThis->showMessage("Delete Failed",
                        "Could not delete the preset file.\n"
                        "It may be read-only or in use by another application.");
                }

                safeThis->refreshAll();
            }));
    }

    // ------------------------------------------------------------------
    // SAVE: サブカテゴリ + プリセット名を入力させる
    // ------------------------------------------------------------------
    void showSaveDialog()
    {
        auto* win = new juce::AlertWindow("Save Preset",
                                          "Choose a sub-category and enter a preset name.\n"
                                          "The sample file paths of all 8 slots are stored with the preset.",
                                          juce::MessageBoxIconType::NoIcon,
                                          this);

        juce::StringArray cats;
        if (getCategories) cats = getCategories();
        if (cats.isEmpty()) cats.add("User");

        win->addComboBox("category", cats, "Sub-Category");

        if (auto* cb = win->getComboBoxComponent("category"))
        {
            cb->setEditableText(true);   // 既存から選ぶ / 新規に打ち込む の両対応
            cb->setSelectedItemIndex(0, juce::dontSendNotification);
        }

        win->addTextEditor("name", lastSavedName, "Preset Name");
        win->addButton("OK",     1, juce::KeyPress(juce::KeyPress::returnKey));
        win->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        juce::Component::SafePointer<PresetBrowser> safeThis(this);

        win->enterModalState(true, juce::ModalCallbackFunction::create(
            [safeThis, win](int result)
            {
                // AlertWindow は new で作っているのでここで必ず破棄する
                std::unique_ptr<juce::AlertWindow> owned(win);
                if (result != 1 || safeThis == nullptr) return;

                juce::String cat = "User";
                if (auto* cb = owned->getComboBoxComponent("category"))
                {
                    const auto t = cb->getText().trim();
                    if (t.isNotEmpty()) cat = t;
                }

                const auto name = owned->getTextEditorContents("name").trim();
                if (name.isEmpty())
                {
                    safeThis->showMessage("Save Preset", "Please enter a preset name.");
                    return;
                }

                safeThis->lastSavedName = name;
                if (safeThis->onSaveRequested) safeThis->onSaveRequested(cat, name);
                safeThis->refreshAll();
            }), false);
    }

    // ------------------------------------------------------------------
    // INIT: 破壊的操作なので必ず確認を取る
    // ------------------------------------------------------------------
    void showInitDialog()
    {
        juce::Component::SafePointer<PresetBrowser> safeThis(this);

        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon,
            "Reset to Initial State",
            "This will remove the loaded samples from ALL 8 slots and reset every "
            "parameter to its default value.\n\n"
            "This cannot be undone. Do you want to continue?",
            "Yes",
            "No",
            this,
            juce::ModalCallbackFunction::create([safeThis](int result)
            {
                if (result == 1 && safeThis != nullptr && safeThis->onInitConfirmed)
                    safeThis->onInitConfirmed();
            }));
    }

    struct SimpleListModel : public juce::ListBoxModel
    {
        juce::StringArray items;
        PresetBrowser* owner = nullptr;
        bool isCategoryList = false;

        int getNumRows() override { return items.size(); }

        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (row < 0 || row >= items.size()) return;   // 範囲外描画のガード

            if (selected) { g.setColour(PicoColors::mint.withAlpha(0.2f)); g.fillRect(0, 0, w, h); }
            g.setColour(selected ? PicoColors::mint : juce::Colours::white);
            g.drawText(items[row], 10, 0, w - 20, h, juce::Justification::centredLeft, true);
        }

        void listBoxItemClicked(int row, const juce::MouseEvent& e) override
        {
            if (owner == nullptr) return;

            if (isCategoryList)
            {
                owner->categoryRowClicked(row);
                return;
            }

            if (e.mods.isPopupMenu())
                owner->presetRowRightClicked(row);
        }

        // 行のない余白を右クリックした場合は何もしない (行が無くてもJUCEは呼ぶ)
        void backgroundClicked(const juce::MouseEvent&) override {}

        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
        {
            if (owner == nullptr || isCategoryList) return;
            owner->presetRowClicked(row);
        }

        void returnKeyPressed(int row) override
        {
            if (owner == nullptr || isCategoryList) return;
            owner->presetRowClicked(row);
        }
    };

    juce::TextEditor txtSearch;
    juce::TextButton btnSave, btnInit, btnClose;
    juce::ListBox lstCategories, lstPresets;

    SimpleListModel categoryListModel, presetListModel;

    juce::StringArray categories;
    juce::Array<juce::File> currentPresets;
    int selectedCategoryRow = 0;
    juce::String lastSavedName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
