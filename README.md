<img src="https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/Logos/OpenCore_with_text_Small.png" width="200" height="48"/>

[![Build Status](https://travis-ci.com/acidanthera/OpenCorePkg.svg?branch=master)](https://travis-ci.com/acidanthera/OpenCorePkg) [![Scan Status](https://scan.coverity.com/projects/18169/badge.svg?flat=1)](https://scan.coverity.com/projects/18169)
-----
Additional features/changes implemented by this fork
============

[ Multi-Boot ]
         
         - ACPI patches are optional for non macOS with setting ACPI->Quirks->EnableForAll to yes (default is no).
         - Booter Quirtks, SMBIOS and Device Properties patches will only applied to macOS.
 
[ Hotkeys ]
 
         - Full functional Hotkeys [1-9] corresponding to Boot Entry's Index number and dedicated W (Windows) / X (macOS) keys can be used without seeing Boot Picker.
          
[ Ui Boot Picker ]
              
          - Bios Date/time, auto boot to the same OS or manual set to always boot one OS mode, and OC version are displayed in boot picker.
          - Auto boot to previous booted OS (if Misc->Security->AllowSetDefault is NO/false).
          - macOS Recovery/Tools Entries are hidden by default, use Spacebar in Boot Menu as a toggle on/off to show/hide hidden entries.
          
[ Custom Entries ]
 
          - Custom entries are now listed first in picker menu and by the orders they are appeared in Misc->Boot->Entries, before all other entries.
          - Individual custom entry can be set hidden using Misc->Entries->Item 0->Hidden (Boolean).
          - Ability to change entry name found by auto scanner by adding custom entry with the exact same device path, this will give users the option to complete change how all boot entries listed in Boot Picker.
    
[ Others ]

          - No verbose apfs.efi driver loading (if using apfs.efi instead of ApfsDriverLoader.efi).
          - Fixed the unmatched 1st and 2nd stages boot Apple logo (* To ensure a match, set Misc->Boot->Resolution to match with one in macOS preferences, and to better boot menu text visibility for 4k+ display, set Misc->Boot->ConsoleMode to Max).
          - ndk-macbuild.tool script are set to compile with latest edk2 (One can easily set to stable edk2 if prefer).
          - NvmExpressDxe driver build script are also available for system without native nvme support. (Compatible with OC and Clover).



 Usage:
- To build OpenCore, run "./ndk-macbuild.tool" at Terminal (require Xcode and Xcode Command Line Tool installed, and open xcode to accept license agreement before compiling).
- To build NvmExpressDxe driver, run "./buildnvme.sh".



## Discussion

- [AppleLife.ru](https://applelife.ru/threads/razrabotka-opencore.2943955) in Russian
- [Hackintosh-Forum.de](https://www.hackintosh-forum.de/forum/thread/42353-opencore-bootloader) in German
- [InsanelyMac.com](https://www.insanelymac.com/forum/topic/338527-opencore-development/) in English
- [MacRumors.com](https://forums.macrumors.com/threads/opencore-on-the-mac-pro.2207814/) in English, legacy Apple hardware
- [macOS86.it](https://www.macos86.it/showthread.php?4570-OpenCore-aka-OC-Nuovo-BootLoader) in Italian
- [PCbeta.com](http://bbs.pcbeta.com/viewthread-1815623-1-1.html) in Chinese

## Credits

- The HermitCrabs Lab
- All projects providing third-party code (refer to file headers)
- [al3xtjames](https://github.com/al3xtjames)
- [Andrey1970AppleLife](https://github.com/Andrey1970AppleLife)
- [Download-Fritz](https://github.com/Download-Fritz)
- [Goldfish64](https://github.com/Goldfish64)
- [PMHeart](https://github.com/PMHeart)
- [savvamitrofanov](https://github.com/savvamitrofanov)
- [vit9696](https://github.com/vit9696)
