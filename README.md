# Phase Width 4 - VST3 plugin

Four-band dynamic stereo width / phase processor, ported from the Max patch.
Three crossovers split the signal into four bands. Per band:

* **phase target** slider: 0 = collapse to mono, 0.5 = no effect, 1 = push out of phase
* **amount** dial: how far the band is moved towards the target, tracking the incoming signal
* **attack / decay** (0 - 2 s): ballistics of that movement, like a compressor
* **solo**, measured **input** and **output phase** bars, a **change** display (width change in dB with
  input/output markers) and an output **goniometer**
* Global: **bypass**, **goniometers on/off**, **MASTER RESET ALL**, input/output peak meters.

Reset buttons (or double-click on a control) return it to its default.

## Building it without installing anything (GitHub does it for you)

1. Make a free account at github.com and sign in.
2. Click **+** (top right) > **New repository**. Name it `phase-width-4`, choose **Public**, click **Create repository**.
3. On the next page click **uploading an existing file**. Unzip the project folder on your Mac, then drag
   **everything inside it** into the browser (the `Source` and `tests` folders, `CMakeLists.txt`,
   `README.md`, `.gitignore` and the hidden `.github` folder - see the note below if you can't see it).
   Click **Commit changes**.
4. Open the **Actions** tab. Choose **Build Phase Width 4 (macOS VST3)** on the left and click **Run workflow**
   (it also runs by itself after your upload). It takes about 10 - 15 minutes.
5. When the run has a green tick, open it and download **PhaseWidth4-VST3-mac** from *Artifacts* at the bottom.
6. Unzip it and put `Phase Width 4.vst3` in `/Library/Audio/Plug-Ins/VST3/` (all users) or
   `~/Library/Audio/Plug-Ins/VST3/` (just you). In Finder, **Go > Go to Folder...** and paste the path.
7. Restart Nuendo, then re-scan plug-ins if asked: **Studio > VST Plug-in Manager**. The plugin is under *Fx*.

**Hidden `.github` folder:** in Finder press **Cmd + Shift + .** to show hidden files so you can drag it too.
If the upload page refuses a folder, you can instead create the file in the browser: **Add file > Create new file**,
type `.github/workflows/build.yml` as the name and paste the contents.

**If macOS says the plugin can't be opened** (it is not notarised): open Terminal and run
`xattr -cr "/Library/Audio/Plug-Ins/VST3/Phase Width 4.vst3"` (use the right path), then rescan.

**If the build fails:** open the failed run, click the red step, and send me the last 30 lines of the log.

## Notes
* Stereo in, stereo out only. No latency is reported (the processing is zero-latency).
* The goniometer picture is auto-scaled so quiet bands are still readable.
* Band 4 is "input minus lowpass 3" (like the Max patch), so low frequencies leak into it at about -25 dB;
  soloing it is not a perfect highpass. That is the price of the four bands summing back flat exactly.

## Running the audio tests yourself (optional, needs a C++ compiler)
`cd tests && g++ -O2 -std=c++17 -I../Source test_dsp.cpp -o test_dsp && ./test_dsp`
