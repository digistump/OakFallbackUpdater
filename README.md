# OakFallbackUpdater
Sketch used to generate the Oak fallback system updater

NOTE: This code gets compiled and then linked and packed into the second half of the system bin file, it is only activated if your Oak totally fails at connecting to Particle repeatedly due to a bad system update (which is already CRCed in two ways to avoid failure).

**REALLY YOU DON'T WANT TO PUSH THIS OVER OTA TO YOUR OAK, IT WILL LIKELY BRICK IT**

Instructions to build:

- Install ESP8266 Core 2.0.0 (NOT via board manager)
- Replace boards.txt and platform.txt with the ones found here
- Add oak_half2.ld to tools/sdk/ld in the files for that core.
- Copy libmain.a from here over the one at tools/sdk/lib in the files for that core.
- Put esptool2 executable for your platform (download from OakCore releases page) in the tools folder for that core.
- Select "Oak Fallback" from boards list
- Compile
- Take bin generated from OakSystem (using the Oak core), pad that to a size of 0x7F000 (good tool for this on windows: http://www.smspower.org/maxim/forumstuff/padbin.zip), join this to the end of it (fjoiner works for this on windows).
- You now have a system update file, which might be useful if you are running your own cloud/making your own platform
- Really don't send it over OTA to your Oak