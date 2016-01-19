# OakFallbackUpdater
Sketch used to generate the Oak sfallback system updater

PLEASE DO NOT TRY TO BUILD OR FIX - major refactoring taking place locally, otherwise this is fully tested code (if ugly)

NOTE: This code gets compiled and then linked and packed into the second half of the system bin file, it is only activated if your Oak totally fails at connecting to Particle repeatedly due to a bad system update (which is already CRCed in two ways to avoid failure).