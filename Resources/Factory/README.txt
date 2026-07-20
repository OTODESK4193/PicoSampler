PicoSampler - Factory Content
=============================

Put the files you want to ship WITH the plugin in this folder.
Everything here is compiled into the binary and extracted on first launch,
so the end user can play the preset without hunting for sample files.

HOW TO ADD "BasicPiano"
-----------------------
1. Copy the audio file here, e.g.:
       Resources/Factory/BasicPiano.wav

2. Launch PicoSampler, load that same WAV, dial in the sound you want.

3. Press PRESETS -> SAVE.
       Sub-Category : Factory
       Preset Name  : BasicPiano

4. Copy the file that was just written:
       %APPDATA%\PicoSampler\Presets\Factory\BasicPiano.picopreset
   into this folder:
       Resources/Factory/BasicPiano.picopreset

5. Re-run CMake configure (so the new files are picked up) and rebuild.

WHAT HAPPENS AT RUNTIME
-----------------------
The AUDIO is never written to disk. It stays inside the plugin binary and is
loaded straight from memory. Inside the preset it is referenced by a virtual
path:

    factory://BasicPiano.wav

Only the preset definition is written out, so the browser can list it:

    %APPDATA%\PicoSampler\Presets\Factory\BasicPiano.picopreset

When installing, any absolute path in the preset (e.g. D:\VST_Project\...)
is rewritten to the factory:// form if a resource with the same file name is
embedded. Without this the preset would report "Missing Samples" on every
machine except the one it was made on.

Because the audio lives in the binary, a factory preset can never break:
the user cannot move or delete the sample.

Factory presets are refreshed on every launch, so a plugin update also
updates them. To tweak one, save it under a different name or sub-category.

NOTE ON FILE SIZE
-----------------
The audio is embedded in the binary, so a 50 MB piano makes a 50 MB plugin.
Keep BasicPiano small - a short mono/stereo one-shot is ideal, since
PicoSampler's stretch anchors handle the pitch range.
