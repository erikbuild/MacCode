/* ABOUTME: Load/save the proxy server IP+port to a "MacCode Prefs" file in the
   ABOUTME: System Folder's Preferences folder. Keeps gApp defaults if absent/invalid. */
#ifndef PREFS_H
#define PREFS_H
void PrefsLoad(void);   /* read serverIP/serverPort into gApp (no-op if no/invalid file) */
void PrefsSave(void);   /* write gApp's serverIP/serverPort to the prefs file */
#endif
