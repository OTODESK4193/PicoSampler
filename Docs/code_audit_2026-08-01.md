# PicoSampler コード検証レポート

> **実装状況** (2026-08-01 更新)
> - ✅ 段階1 完了: 項目 1〜6, 9, 13, 14 + AutoSlice のスレッド越しパラメータ書き込み
> - ✅ 段階2 完了: 項目 7, 10, 11 + FX 全パラメータのスムージング
>   + 追加発見・修正: **Sat Pre-HPF が未実装だった**（ノブも MOD 先もあるのに DSP が値を使っていなかった）
>   + 訂正: Master HPF/LPF は `TptSvfFilter` が内部で係数を平滑化済みだった（対応不要）
> - ⬜ 段階3 未着手: 項目 8, 12（Arp Rate MOD / Arc 倍率）


作成日: 2026-08-01 / 対象コミット: `4c35009` (Filterカーブの縮尺を修正)
対象: Source/ 配下 全 10,692 行（third_party 除く）

> 方針: **確立済みロジックには手を入れず、検証と原因特定のみ**を行いました。
> 修正案は「こう直すと安全」という提案であり、まだ適用していません。

---

## 0. 総評

DSP のアルゴリズム本体（Stretch アンカー、フィルター、ループ X-Fade、Edge Fade）は
よく作り込まれており、過去のコミットで潰してきたバグの再発防止コメントも丁寧です。
一方で **「オーディオスレッド外からの書き込み競合」と「メモリ量」** の 2 点が
まだ構造的に残っており、報告いただいた 2 つのクラッシュはここに起因すると判断しました。

重大度の内訳:

| 重大度 | 件数 | 概要 |
|---|---|---|
| 🔴 Critical | 4 | クラッシュ直結（メモリ枯渇 / データ競合 / 二重ライター） |
| 🟠 High | 5 | 音が出ない・固まる・MOD が効かない |
| 🟡 Medium | 6 | ジッパーノイズ・表示不整合・RT安全性 |
| 🔵 Low | 4 | デッドコード・表記ゆれ |

---

## 1. Reanalyze ボタン連打時のクラッシュ 🔴

### 原因A（本命）: `metadata.filePath` の juce::String データ競合

`PluginEditor.cpp:51-63`

```cpp
btnReAnalyze.onClick = [this] {
    const bool isAutoSlice = ...->load() > 0.5f;
    if (isAutoSlice) {
        const juce::File file = audioProcessor.getSamplerEngine()
                                    .getSlot(0).getMetadata().filePath;   // ★ ここ
```

- `getMetadata().filePath` は **`isReady()` チェックなしで** メッセージスレッドから読まれます。
- 同時にローダースレッドは `SampleSlot.cpp:60` で
  `metadata.filePath = pathForMetadata;` と**書き込んでいます**。
- `juce::String` は内部でヒープ文字列を参照カウントで共有します。
  代入中（旧文字列の参照カウントを減らして解放 → 新ポインタを書き込む）に
  別スレッドが古いポインタを読むと **解放済みメモリのコピー = クラッシュ**。
- 「解析が終わる前に押す」ほど当たりやすい ＝ ご報告の再現条件と完全に一致します。

**修正案**: ボタン側で `isReady()` を確認したうえで、パスは
`std::atomic` 化した別変数か、`SampleSlot` 側に `getFilePathSafe()`
（ReadGuard を取ってからコピーを返す）を新設して取得する。

### 原因B: 同一スロットへのジョブが無制限に積み上がる

`PluginProcessor.cpp:705-723` — `reanalyzeSlot()` は押した回数だけ
`pendingJobs` に積みます。1 ジョブあたり 49 アンカー分の time-stretch を
やり直すため、10 回押せば 10 回分（長尺サンプルなら数分）走り続けます。
その間ずっと `analyzing=true` で GUI は「Analyzing...」のまま。

**修正案**: `pendingJobs` 追加前に「同一 slotIndex の Reanalyze ジョブが
既にあれば置き換える（重複排除）」を入れる。3 行で済みます。

```cpp
const juce::ScopedLock lock(jobLock);
for (int i = pendingJobs.size(); --i >= 0;)
    if (pendingJobs[i].type == JobType::Reanalyze && pendingJobs[i].slotIndex == slotIdx)
        pendingJobs.remove(i);
pendingJobs.add(job);
```

### 原因C: AutoSlice ジョブが `analyzing` を戻さない

`PluginProcessor.cpp:1024-1026` の `case JobType::AutoSlice:` には
`setAnalyzing(false)` がありません。`autoSliceFile()` は
`554: if (numSamples < 100 || numChIn < 1) return;` で早期 return しうるので、
**壊れたファイル / 極短ファイルを AutoSlice ON で落とすと「Analyzing...」が永久に残ります**
（`WaveformDisplay::paint` が `isAnalyzing()` で早期 return するため波形も出ない）。

---

## 2. 各 Slot への連続 D&D クラッシュ 🔴

### 原因A（本命）: アンカーバッファのメモリ量

`SampleSlot.h:17` `kNumAnchors = 49` — **原音と同じ長さのバッファを 49 本**、
スロットごとに保持します。しかも `isStretchMode` の ON/OFF に関わらず
ロード時に必ず全部生成します。

| サンプル長 (48kHz ステレオ) | 1 スロット | 8 スロット全部 |
|---|---|---|
| 0.5 秒（ワンショット） | 約 9 MB | 約 75 MB |
| 2 秒 | 約 38 MB | 約 300 MB |
| **8 秒（1〜2小節ループ）** | **約 150 MB** | **約 1.2 GB** |
| 30 秒 | 約 565 MB | 約 4.5 GB |

`SampleSlot.cpp:175` の `anchorBuffers[i].setSize(numCh, numSamples);` は
確保失敗時に `std::bad_alloc` を投げますが、**どこにも catch がありません** →
即プロセス終了（DAW ごと落ちる）。

「各 Slot に連続して D&D」＝ まさに 8 スロット全部を埋める操作なので、
数秒以上のサンプルを使った時点で高確率で当たります。

**修正案（いずれか）**:
1. **ロード長の上限を設ける**（例: 30 秒を超える分は警告して切り詰め）
2. **アンカーを遅延生成**にする（`isStretchMode` が ON のスロットだけ、
   さらに実際に使われた半音だけ生成）— 効果は最大だが変更は中規模
3. 最低限、`setSize` を `try/catch (std::bad_alloc)` で囲み、
   失敗時は `analyzing=false` にしてそのスロットを空扱いにする（安全網）

### 原因B: 「書き手が 2 人」になる経路が塞がれていない

`SampleSlot::beginBufferWrite()` (`SampleSlot.cpp:9-27`) は
**オーディオスレッドの読み手（readerCount）**からは守りますが、
**別スレッドの書き手同士**は一切排他していません。実際に競合しうる組み合わせ:

| メッセージスレッド | ローダースレッド | 結果 |
|---|---|---|
| `clearSlot()` (波形右クリック→削除) | `finishLoad()` の `reader->read(&originalBuffer,...)` | 同一バッファの解放と書込が同時 → ヒープ破壊 |
| `loadPreset()` の `getSlot(i).clear()` (`:863`) | `renderAnchors()` の `setSize()` | 同上 |
| `resetToInitState()` の `clear()` (`:908`) | 同上 | 同上 |

`pendingJobs.clear()` は**キュー待ちのジョブしか消せず、実行中のジョブは止まりません**。

**修正案**: `SampleSlot` に `juce::CriticalSection writeLock;` を 1 本追加し、
`loadFromFile / finishLoad / reanalyze / copyFrom / clear` の入口で
`ScopedLock` を取る。既存の `ReadGuard`（オーディオスレッド用）とは
役割が別なので共存できます。ロジック本体には触れません。

### 原因C: `startNote()` に ReadGuard がない

`SamplerVoice.cpp:7-98` の `startNote()` は `slot.getMetadata()` /
`slot.getAnchorBuffer()` / `getOriginalBuffer()` を触りますが、
`renderNextBlock()` と違って `ReadGuard` を取っていません
（`SamplerEngine.cpp:140` から呼ばれる）。
`isReady()` は見ていますが、見た直後にローダーが `beginBufferWrite()` すると
すり抜けます。`renderNextBlock` 側と同じ 2 行を入れるだけで塞げます。

---

## 3. MOD アサイン先の実装漏れ ✅ ほぼ無し / ⚠️ 1 件

`ModMatrix::Dst` の全 **119 個**を追跡した結果:

| グループ | 個数 | DSP 適用 | 選択メニュー | Arc 表示 |
|---|---|---|---|---|
| S1–S8 Start/End/L-Start/L-End/X-Fade | 40 | ✅ `PluginProcessor.cpp:388-393` | ✅ | ✅ |
| Master Pitch | 1 | ✅ `:354` | ✅ | ✅ |
| ARP 7 種 | 7 | ⚠️ 下記 | ✅ | ✅ |
| Filter Cutoff/Reso/Formant/CombMix | 4 | ✅ `:457-473` | ✅ | ✅ |
| FX1–5 Amount | 5 | ✅ `:492` | ✅ | ✅ |
| FX 詳細 16 種 | 16 | ✅ `:496-512` | ✅ | ✅ |
| S1–S8 Pan | 8 | ✅ `:383-385` | ✅ | ✅ |
| LFO1–4 Rate | 4 | ✅ `ModMatrix.h:281-283` | ✅ | ✅ |
| Filter Env A/D/S/R | 4 | ✅ `:423-426` | ✅ | ✅ |
| S1–S8 Amp ADSR | 32 | ✅ `:372-376` | ✅ | ✅ |

**未接続の行き先はゼロ**でした。選択メニュー（`ModPanel::buildDestMenu`）にも
抜けはありません。

### ⚠️ ただし: `DstArpRate` は Sync ON のとき無効 🟠

`PluginProcessor.cpp:308`

```cpp
arpParams.rateFreeHz = jlimit(1.0f, 30.0f, arpParams.rateFreeHz + modMatrix.get(DstArpRate) * 15.0f);
```

MOD は `rateFreeHz` にしか乗りません。しかし `Arpeggiator.cpp:358-362` は

```cpp
if (p.sync) { secPerStep = (60.0 / bpm) * beatsTable[p.rateSync]; }  // rateFreeHz を見ない
```

`arpSync` の**既定値は true**（`PluginProcessor.cpp:129`）なので、
初期状態で Arp Rate に MOD をアサインしても**何も起きません**。
Arc も `knobRateFree` に描かれており、Sync 中はそのノブ自体が隠れます。

**修正案**: Sync 時は `beatsTable` のインデックスを MOD でずらす
（`rateSync + (int)std::round(mod * 6.0f)` を `jlimit(0,12,...)`）か、
Sync 中は Rate アサインを無効と明示する。

---

## 4. MOD 範囲表示（Arc）の漏れ ✅ 描画漏れ無し / ⚠️ スケール不一致 🟡

`PluginEditor::timerCallback` (`:427-496`) が全 Dst グループを網羅しており、
**Arc が描かれないノブはありません**。`ArcDial.cpp:35-53` の帯描画、
`:69-81` のライブドットも正しく `mod_active` で切り替わっています。

ただし `updateKnobProps` (`:393-425`) は

```cpp
props.set("mod_max", jlimit(0.0f, 1.0f, normVal + maxOffset));
```

と **正規化値（0–1）に MOD 量をそのまま足して**います。一方 DSP 側は
行き先ごとに固有の倍率を掛けています。この 2 つが一致していないため、
下記の行き先では**帯の幅が実際の変調量と合いません**。

| 行き先 | DSP の倍率 | Arc の見え方 |
|---|---|---|
| Master Pitch | ×24 半音 | 実際よりかなり狭く見える |
| Filter Cutoff | ×4 オクターブ（指数） | 全く別物（線形 0–1 の帯） |
| Filter Reso | ×5 | 狭く見える |
| Arp Rate / Octaves / Offset / Repeat | ×15 / ×3 / ×12 / ×3 | 不一致 |
| Amp & Filter Env Attack/Decay | ×2.5 秒 | 不一致 |
| Amp & Filter Env Release | ×5 秒 | 不一致 |
| Sat Drive / Pre-HPF / Trim | ×6 / ×1000 / ×12 | 不一致 |
| Chorus Rate | ×2 | 不一致 |
| Freeze Size | ×500 | 不一致 |

一致しているのは **0–1 レンジの行き先のみ**
（Start/End/L-Start/L-End/X-Fade、Pan、FX Amount、Depth/Width/Feedback/
Duck/Damp/Sustain/Formant/CombMix/Decay/Shimmer/Mod）。

**修正案**: `Dst → 倍率` のテーブルを 1 つ用意し、`updateKnobProps` に
`float scale` 引数を追加。`normVal + knob.valueToProportionOfLength(value + offset*scale) - normVal`
のように**値空間で足してから正規化**する。DSP 側は一切触らずに済みます。

---

## 5. ノブのスムージング 🟡 大部分が未実装

現状スムージングが入っているのは **3 個だけ**、しかもうち 1 個は未使用です。

| 変数 | 状態 |
|---|---|
| `smoothedCutoff` | 使用中だが **バグあり**（下記） |
| `smoothedReso` | 同上 |
| `smoothedGain` | `prepareToPlay` で reset するだけで**どこにも使われていない**（デッドコード） |

### 🔴→🟠 バグ: ランプがブロックあたり 1 サンプルしか進まない

`PluginProcessor.cpp:463-469`

```cpp
smoothedCutoff.setTargetValue(targetCutoff);
smoothedReso.setTargetValue(targetReso);
fltParams.cutoff = smoothedCutoff.getNextValue();   // ★ ブロックに 1 回だけ
fltParams.res    = smoothedReso.getNextValue();
```

`reset(sampleRate, 0.02)` は「**サンプル**単位で 20ms 分」＝ 44.1kHz なら 882 ステップ。
それを 1 ブロック 1 回しか進めないので、実効スムージング時間は

> 0.02 秒 × バッファサイズ ＝ **512 サンプル設定なら約 10 秒**

Cutoff ノブを回してから実際に音が変わりきるまで 10 秒かかる計算です。
これは `filterAdsr` で既に潰したバグ（`:429` のコメント）と**全く同じパターン**です。

**修正案**: ブロックレートで使うなら
`smoothedCutoff.skip(numSamples - 1); fltParams.cutoff = smoothedCutoff.getNextValue();`
または `reset(sampleRate / samplesPerBlock, 0.02)` でブロックレート換算にする。

### スムージング未実装のパラメータ

| 場所 | パラメータ | 症状 |
|---|---|---|
| `SamplerEngine.cpp:221` | `masterGain` (Out Gain) | ノブを回すとジッパーノイズ |
| `SamplerEngine.cpp:211-214` | Master HPF / LPF | 同上、カットオフ跳びでプチノイズ |
| `SamplerVoice.cpp:228-230` | `pan` / `slotGainDb` | **Pan は MOD 先**なので LFO をかけるとブロック段差が出る |
| `PluginProcessor.cpp:472-473` | Filter Formant / CombMix | MOD 時に段差 |
| `FxChain.h` | Sat Drive / Chorus Rate・Depth / Reverb 各種 | Delay・Freeze の Amount/Feedback/Time には 1 次スムージングあり（`:435-455`）だが他は素通し |

**優先度**: Pan（MOD 先で目立つ）→ Out Gain → Master HPF/LPF → FX 詳細、の順。
`juce::SmoothedValue` を 1 サンプルずつ回すのが素直です。

---

## 6. バージョン情報 ❌ v1.1.0 になっていません

| ファイル | 記載 |
|---|---|
| `CMakeLists.txt:9` | `project(PicoSampler VERSION **1.0.0**)` |
| `CMakeLists.txt:40` | `VERSION "**1.0.0**"` |
| `CMakeLists.txt:148` | `PICOSAMPLER_VERSION="**1.0.0**"` |
| `README.md:3` | `badge/release-**v1.0.0**-blue` |
| `Source/GUI/ConfigPanel.cpp:164` | `"PicoSampler **v1.1.0**"` ← GUI だけ 1.1.0 |

**GUI 表示だけが v1.1.0 で、ビルド情報とバッジは v1.0.0 のまま**です。
DAW のプラグイン情報にも 1.0.0 が出ます。上記 4 箇所を 1.1.0 に更新してください。

---

## 7. その他 堅牢性・安定性

### 🟡 processBlock 内でヒープ確保（RT 安全性違反）

`PluginProcessor.cpp:440-442`

```cpp
juce::AudioBuffer<float> fltBypassBuffer(numChannels, numSamples);
juce::AudioBuffer<float> fxBypassBuffer(numChannels, numSamples);
juce::AudioBuffer<float> bothBypassBuffer(numChannels, numSamples);
```

**毎ブロック 3 回 malloc/free** しています。オーディオスレッドでの
ヒープ確保はロックを伴いうるため、低レイテンシ設定でのドロップアウト要因です。
メンバに持って `prepareToPlay` で `setSize(2, samplesPerBlock)`、
`processBlock` では `clear()` するだけにするのが定石です。

### 🟠 `autoSliceFile` がローダースレッドから `setValueNotifyingHost`

`PluginProcessor.cpp:656-701` — 8 スロット分のパラメータをローダースレッドから
`setValueNotifyingHost()` しています。APVTS のアタッチメントは AsyncUpdater 経由で
メッセージスレッドに戻すので概ね安全ですが、ホストのオートメーション記録側は
メッセージスレッド想定です。`juce::MessageManager::callAsync` でまとめて
メッセージスレッドに投げるのが安全です。

### 🟡 `beginBufferWrite` のスピン上限後にそのまま書いてしまう

`SampleSlot.cpp:17-26` — 2000 回（実質約 2 秒）待って抜けなければ
`jassertfalse` だけして **return せずに呼び出し元へ戻り、書き込みが続行**されます。
Debug ではアサートで気づけますが、Release では**読み手がいる状態で解放**します。
戻り値 `bool` にして、false なら書き込みを諦める形が安全です。

### 🟡 `filterAdsr` を numSamples 回まわして最後の値だけ使う

`PluginProcessor.cpp:433-435` — 進み方は正しくなりましたが、
使うのは**ブロック最後の 1 値だけ**なので Cutoff への反映はブロックレートです。
上記 5. のスムージング修正と合わせると自然になります。

### 🔵 `SlotPanel` が完全なデッドコード

`CMakeLists.txt:84-85` でビルド対象ですが、`SlotPanel` を
`#include` / インスタンス化している箇所は**どこにもありません**。
しかも `SlotPanel.cpp:20` は

```cpp
samplerEngine.getSlot(idx).loadFromFile(file);   // メッセージスレッドで直接ロード!
```

と、ジョブキューを迂回して**メッセージスレッドから直接** 49 アンカー生成を
呼んでいます。今は使われていないので無害ですが、将来 UI に戻した瞬間に
GUI フリーズ＋二重ライタークラッシュになる**地雷**です。削除を推奨します。

### 🔵 その他の小さな指摘

- `PluginProcessor.cpp:270` `polyVals[jlimit(0,5,polyChoice)]` — 境界チェック済み ✅
- `ModMatrix.h:243` `if (--heldNotes <= 0)` — Note-Off が余分に来ても 0 でクランプ ✅
- `WaveformDisplay.cpp:346` ゼロ交差探索が最悪 `numSamples` 回ループ。
  スナップ ON で長尺 DC オフセット波形をドラッグすると 1 フレーム重くなる（実害は軽微）
- `SamplerEngine.cpp:220` / `SamplerVoice.cpp:234` のモノラル時 `outR = outL` 共有 —
  Engine 側は最後に代入（`outL[s]=l; outR[s]=r;`）なので問題なし ✅。
  ただし Voice 側は `outL[s] += ...; outR[s] += ...;` と**加算**なので、
  モノ出力バスのとき同じサンプルに 2 回足され**音量が約 2 倍**になります。
  `isBusesLayoutSupported` がモノを許可している（`:246`）ため実際に起こりえます
- `PitchAnalyzer` は `static` 関数のみで状態を持たないためスレッドセーフ ✅

---

## 8. 推奨対応順

| # | 項目 | 重大度 | 想定工数 |
|---|---|---|---|
| 1 | バージョン 4 箇所を 1.1.0 に統一 | ❌ | 5 分 |
| 2 | `SampleSlot` に writeLock を追加（書き手同士の排他） | 🔴 | 30 分 |
| 3 | Reanalyze ボタンの `filePath` 取得を安全化 | 🔴 | 15 分 |
| 4 | `setSize` の `std::bad_alloc` 安全網 + ロード長上限 | 🔴 | 30 分 |
| 5 | Reanalyze ジョブの重複排除 | 🟠 | 10 分 |
| 6 | AutoSlice ジョブの `setAnalyzing(false)` 漏れ | 🟠 | 5 分 |
| 7 | `smoothedCutoff/Reso` のランプ進行を修正 | 🟠 | 10 分 |
| 8 | Arp Rate MOD を Sync 時にも効かせる | 🟠 | 20 分 |
| 9 | `startNote()` に ReadGuard | 🟠 | 5 分 |
| 10 | processBlock のバッファをメンバ化 | 🟡 | 20 分 |
| 11 | Pan / Out Gain / Master HPF-LPF のスムージング | 🟡 | 1 時間 |
| 12 | Arc の倍率テーブル対応 | 🟡 | 1 時間 |
| 13 | `SlotPanel` 削除 | 🔵 | 5 分 |
| 14 | アンカー遅延生成（根本的なメモリ削減） | 🔴 中長期 | 半日〜 |
