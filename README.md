<img src="https://github.com/acidanthera/OpenCorePkg/blob/master/Docs/Logos/OpenCore_with_text_Small.png" width="200" height="48"/>

[![Build Status](https://travis-ci.com/acidanthera/OpenCorePkg.svg?branch=master)](https://travis-ci.com/acidanthera/OpenCorePkg) [![Scan Status](https://scan.coverity.com/projects/18169/badge.svg?flat=1)](https://scan.coverity.com/projects/18169)
-----
Additional features implemented by this fork
============

  OpenCore bootloader front end.

- Hotkey W to boot directly to first available Windows boot entry from either auto scanner or custom entries. (Hold down W to boot Windows OS directly).
- Auto default boot to last booted macOs or Windows.
- No verbose apfs.efi driver loading (if using apfs.efi instead of ApfsDriverLoader.efi).
- Avoid duplicated entry in boot menu, cusstom entry will not be added to boot menu if the same entry already found by auto scanner.
- Ability to change entry name found by auto scanner by adding custom entry with the exact same device path.
- Compile with latest edk2.
- NvmExpressDxe driver build script are also available for system without native nvme support. (Compatible with OC and Clover).
- ACPI patches is now optional for non macOS with setting ACPI->Quirks->EnableForAll to yes (default is no).
- Boot entry can be switched and selected with Up/Down arrow and Enter keys.
- Fixed the unmatched 1st and 2nd stages boot Apple logo (* To ensure a match, set Misc->Boot->Resolution to match with one in macOS preferences, and to better boot menu text visibility for 4k+ display, set Misc->Boot->ConsoleMode to Max).

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
