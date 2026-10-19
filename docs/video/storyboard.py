"""Narration and source ledger for the beginner setup movie."""
SCENES = [
('welcome','From an empty library to your PS4','server-home','browser',[
 'Welcome to PS4 Games Installer. This guide connects a library on your computer to your already jailbroken PlayStation four.',
 'We will start a real Docker Compose server, check its library in a browser, install the PS4 application, and enter the library address.',
 'This edition uses Ubuntu Linux. Pause whenever you need to copy a command. A companion text file contains the commands and addresses.'],
 ['Ubuntu Linux + Docker Compose','PS4 with GoldHEN already working','Beginner guide: pause and follow along']),
('requirements','What you need before starting','server-project','browser',[
 'You need a computer that stays on while downloading, a PS4 with working GoldHEN, and access to the same home network.',
 'Have your own compatible package files ready. This tutorial does not provide commercial games or perform the jailbreak. Do not update the PS4 firmware to follow it.',
 'A USB stick is useful for the one-time application installation. Afterwards, the app reads the library over your network.'],
 ['Same router / home network','Existing jailbreak and active GoldHEN','Your own compatible .pkg files']),
('docker-install','Install Docker on Ubuntu','docker-instructions','browser',[
 'If Docker is already installed, move to the next chapter. Otherwise, open the official Docker Ubuntu installation page shown here.',
 'Open Terminal with Control, Alt, T. Follow the section called Install using the apt repository. The companion file includes these official commands.',
 'First add the Docker signing key and package repository. Then install Docker Engine, its command line tools, and the Compose plugin.',
 'Commands beginning with sudo may ask for your computer password. Nothing appears while typing that password. Press Enter when finished.',
 'This recording computer already has Docker, so it is not reinstalled during filming. The next steps are recorded from real command execution.'],
 ['docs.docker.com/engine/install/ubuntu/','Install Engine + Compose plugin','Computer password is hidden while typing']),
('verify','Check that Compose works','docker-version','terminal',[
 'Run docker compose version. You should see a version number, as this real terminal does. Compose means the two words docker and compose, separated by a space.',
 'If the command is missing, finish installing the Compose plugin first. If Docker reports permission denied, use sudo with the Docker commands.',
 'You do not need Node or programming tools installed on the computer. The container supplies the server software.'],
 ['docker compose version','Version number = Compose is available','Use sudo if Docker access requires it']),
('network','Find the computer’s network address','network','terminal',[
 'Run hostname, a space, then a dash and capital I. This lists the computer network addresses.',
 'In this recording the address reachable on our home network is ten dot one dot fourteen dot ten. Your computer will have a different address.',
 'Use the address of the computer connected to the same router as the PS4. Do not put localhost or a Docker container address into the PS4.',
 'Keep this server on your trusted home network. You do not need to forward router ports or expose it to the internet.'],
 ['Recording LAN address: 10.1.14.10','Use YOUR computer’s LAN address','Never use localhost on the PS4']),
('folders','Create the library folders','folders','terminal',[
 'Create a folder called ps4-library. Inside it, create data, then pkgs, covers, and json, exactly as shown.',
 'The games subfolder sits inside pkgs. Put game packages in that category folder, rather than dropping them into the top level of pkgs.',
 'The covers folder stores icons. The json folder stores the generated library and scan cache. These folders stay on your computer when the container is replaced.'],
 ['ps4-library/data/pkgs/games','ps4-library/data/covers','ps4-library/data/json']),
('compose','Download the real Compose example','compose-file','terminal',[
 'Change into ps4-library, then download the Compose example from the project. The command saves it as compose dot yaml.',
 'Compose reads this file to find the server image, network ports, and folders. Use the official project link in the companion notes.',
 'Our example uses the published nulldevil server image. There is no application to compile, and the package folder is mounted read-only inside the container.'],
 ['Save the official example as compose.yaml','Run later commands from ps4-library','Image: nulldevil/fpkgi-node-server']),
('environment','Set the address and ports','environment','terminal',[
 'Create the file named dot env beside compose dot yaml. The leading dot is part of its name. Copy the block shown here.',
 'Replace only the computer address with your own. This recording uses port eighteen thousand eighty for package files, and thirteen thousand for the library page.',
 'These alternative ports avoid a service already using eighty eighty on this computer. Keep the same port numbers throughout this guide.',
 'The public address ends with eighteen thousand eighty. The scan interval is three hundred seconds, meaning five minutes. Optional online metadata credentials are not needed.'],
 ['FPKGI_PUBLIC_ADDRESS=YOUR_LAN_IP:18080','Package files: 18080 | Library/JSON: 13000','No IGDB account is required']),
('packages','Put a real package in the library','sample-package','terminal',[
 'Copy your compatible dot pkg files into data, pkgs, games. Packages can also live inside collection subfolders under games.',
 'For this recording, we download the project’s own public installer package. It is a small homebrew example that proves scanning and icon extraction work.',
 'Do not reinstall a running copy of the installer from its own library. Use your own other packages for normal installations.',
 'A zip file is not a package. If a download is compressed, extract the dot pkg first. Download waits in the recording may be shortened.'],
 ['Copy .pkg files into data/pkgs/games/','Recorded example: project’s own public PKG','Do not reinstall the running installer']),
('validate','Check the Compose configuration','validate-compose','terminal',[
 'Use the configuration-check command shown here. The project name, ps4-setup-video, keeps this setup separate. Keep that same project-name option in all the following Compose commands.',
 'No error and a returning prompt means the configuration was accepted. If an address variable is missing, check dot env is in this same folder.',
 'Keep folder names and capitalization exactly as shown. You can always open the companion commands beside the video.'],
 ['docker compose -p ps4-setup-video config --quiet','Returning prompt without error = valid','compose.yaml and .env belong together']),
('start','Start the container','start-server','terminal',[
 'Run the up command shown here, keeping the project-name option, and ending with up, dash d. Compose downloads the image if needed and starts the server in the background.',
 'The first download can take a few minutes. Leave the terminal open until the command finishes. The server scans your category folders at startup.',
 'You only need one server container. If a port or container name is already in use, follow the troubleshooting chapter before trying again.'],
 ['docker compose -p ps4-setup-video up -d','First start downloads the image if necessary','Initial package scan starts automatically']),
('health','Make sure the server is healthy','server-health','terminal',[
 'Run the ps command shown here, with the same project-name option. Look for the server and its healthy status. Starting can appear briefly while the first checks run.',
 'The ports shown in the real output should include our library port thirteen thousand and package port eighteen thousand eighty.',
 'Healthy means the server’s own web check passed. We will still test the actual PS4 connection later, because network access from another device can differ.'],
 ['docker compose -p ps4-setup-video ps','Wait for healthy','Check both published ports']),
('logs','Understand the first scan','server-logs','terminal',[
 'To check the scan, run the logs command shown here, keeping the project-name option and ending with logs, two dashes, tail, then eighteen. These are the actual container messages.',
 'They show the package scan and server startup. A large collection takes longer than our one-package example. Let the initial scan finish.',
 'New packages are picked up by the scheduled scan. Restarting the container also starts a fresh scan, while keeping your mounted folders.'],
 ['docker compose -p ps4-setup-video logs --tail 18','Wait for the initial scan to finish','New files are rescanned every 5 minutes']),
('web','Open the library in a browser','server-home','browser',[
 'On the computer, open a browser and enter your computer address, a colon, and thirteen thousand. Start the address with http colon slash slash.',
 'The real server page lists Available Categories. Our example has games. Click the eye icon beside games to browse the parsed library.',
 'If this page will not open, fix the server or port problem now. Do not continue to the PS4 with an address that already fails on the computer.'],
 ['http://YOUR_LAN_IP:13000/','Open the games category','The eye icon opens the browser library']),
('library','Check the package and its icon','server-library','browser',[
 'This is the real browser library generated by the container we just started. Our installer package and its extracted icon are visible.',
 'Check that your expected packages appear. An empty category usually means files are in the wrong folder, unreadable, or the scan has not finished.',
 'Package file links must point to your computer’s reachable address and package port. The PS4 needs access to those links as well as the JSON address.'],
 ['Real container / real scanned package','Check titles, icons and file addresses','Your packages appear after scanning']),
('json','Copy the correct JSON address','library-json','browser',[
 'Return to the category page and open the games link. The data that appears is the JSON game library. JSON is the list the PS4 application reads.',
 'Copy the browser address ending in slash games. In this recording it uses our computer address and port thirteen thousand.',
 'Do not use the eye-icon library page, and do not add dot json on this server. The exact slash games address is what belongs in Games JSON URL.'],
 ['http://YOUR_LAN_IP:13000/games','Use /games — NOT /library/?cat=games','Do not append .json on this server']),
('release','Download PS4 Games Installer','installer-releases','browser',[
 'Open the PS4 Games Installer GitHub project and choose Releases. Download the newest public file ending in dot pkg.',
 'Choose the package asset, not the source-code zip. At recording time, r forty is the latest public release; your available release may be newer.',
 'The rest of the guide explains the setup shared by these releases. Newer menus can contain additional options without changing the library address you enter.'],
 ['github.com/e1z0/PS4-Games-Installer/releases','Download PS4Game_installer-rN.pkg','Choose the PKG asset, not Source code']),
('usb','Move the installer package to USB','installer-releases','browser',[
 'On your computer, copy the downloaded installer package to the top level of a USB stick, meaning outside any subfolder.',
 'An exFAT file-storage stick is suitable. Formatting erases a drive, so use a prepared spare stick rather than formatting a disk containing important files.',
 'Safely eject it and connect it to the PS4. This is a package-transfer stick, not a PS4 Extended Storage disk.',
 'USB copying is explained here; no USB device is attached to the recording machine. The next step happens on your PS4.'],
 ['Copy the installer PKG to the USB root','Use an existing exFAT file-storage stick','Safely eject, then connect it to the PS4']),
('goldhen','Install the app through GoldHEN','goldhen-project','browser',[
 'Make sure GoldHEN is active on your already jailbroken PS4. Open the GoldHEN menu, then Debug Settings, and its Package Installer.',
 'If your version has Package Source, choose USB or All. Select the PS4 Games Installer package and confirm installation.',
 'Some older layouts expose Package Installer through the normal Settings, Debug Settings, Game path. Menu placement depends on the GoldHEN version.',
 'This section explains the console actions using the official project reference; it is not a new recording of the GoldHEN menus.'],
 ['On PS4: GoldHEN → Debug Settings','Package Source: USB or All','Package Installer → select the installer PKG']),
('open-app','Open PS4 Games Installer','Screenshots/20261017_001035_00608253.jpg','console',[
 'When installation finishes, return to the PS4 home screen and launch PS4 Games Installer. Keep GoldHEN active while using the app.',
 'The console pictures in this guide are real project PS4 captures, rather than a newly recorded console session. They show an existing populated library.',
 'Your first launch may be empty until you set the source. That is normal. We will enter the server address next.'],
 ['Real PS4 project capture','First launch may have no games yet','Configure the source before refreshing']),
('options','Open Options, then Settings','Screenshots/20261017_001111_00700425.jpg','console',[
 'Press the controller’s Options button to open the main menu. Use the directional pad or left stick to select Settings, then press Cross.',
 'Cross is the X-shaped button. Circle goes back. Square opens game details, and Triangle opens game actions.',
 'If your menu has more entries than fit on screen, keep scrolling. There is a visible scroll indicator.'],
 ['Options → Settings','X / Cross = select','O / Circle = back']),
('source','Enter Games JSON URL','build/setup-video/frames/app-source.png','preview',[
 'Choose Games JSON URL and press Cross to edit it. Enter the slash games address copied from the browser, using your computer’s real address.',
 'For our recording that address is http, ten dot one dot fourteen dot ten, port thirteen thousand, slash games. Use your own computer address if it differs from this recording.',
 'This close-up is the actual desktop app preview, not console footage. On PS4, enter text using its on-screen keyboard and confirm with the keyboard’s Done button.'],
 ['Games JSON URL: http://YOUR_LAN_IP:13000/games','Use the JSON/library port, not the package port','Desktop app preview; use YOUR address']),
('save','Test the source, then save it','build/setup-video/frames/app-save.png','preview',[
 'Move down to Test connection and press Cross. A successful result should report a valid library and the number of packages found.',
 'Then choose Save settings. Typing an address or testing it does not replace the separate Save settings step.',
 'Go back with Circle, open Options, and choose Refresh library. Select Available on the sidebar. The scanned packages should now appear with names and icons.'],
 ['Test connection','Save settings','Options → Refresh library → Available']),
('install-game','Install a package from Available','Screenshots/20261017_001231_00636497.jpg','console',[
 'Select a package you are entitled to install from Available. Press Cross, review the confirmation, then confirm to add it to Downloads.',
 'Use a package compatible with your console’s firmware. A matching title alone does not mean every update or package version will work.',
 'For more information before installing, press Square for Details, or Triangle for Actions. Updates and downloadable add-ons appear separately from normal games.'],
 ['Available → select a compatible package → X','Review the confirmation before installing','Square = Details | Triangle = Actions']),
('queue','Watch Downloads until Completed','Screenshots/20261017_002539_00456334.jpg','console',[
 'Open Options, then Downloads, to see progress. Downloading, installing, and confirming the installed title are different steps.',
 'Do not assume one hundred percent downloaded means installation has finished. Wait for Completed, and check the PS4 system Downloads if needed.',
 'Keep the app and server running while transferring data. Allow space for the package copy and the installed application. Newer builds can show downloaded packages waiting to install.'],
 ['Options → Downloads','100% downloaded does not mean installed','Wait for Completed']),
('storage','Choose storage and launch installed games','Screenshots/20261017_001051_00880498.jpg','console',[
 'To install to a supported external disk, use Settings, then Installation destination. It must be PS4 Extended Storage, not the ordinary USB stick used earlier.',
 'If the external drive is unavailable, select internal storage or reconnect it. Package staging can still require internal space.',
 'When a game is installed, find it under Installed and press Cross to launch it. The PS4 may ask to close the installer while starting the game.'],
 ['External destination requires PS4 Extended Storage','A transfer USB stick is a different kind of storage','Installed → X to launch']),
('troubleshoot','If the library is empty or unreachable','server-home','browser',[
 'If the library is empty, check the package files are inside data, pkgs, games. Give the scan time to finish and inspect the container logs.',
 'If the PS4 cannot connect, check the computer is on, the container is healthy, and both devices are on the same home network.',
 'A guest Wi-Fi network can isolate devices. Allow the two server ports on your trusted LAN firewall. Never use localhost as the PS4 source.'],
 ['Empty library: category folders + scan logs','Connection failure: address, ports, LAN access','Avoid guest-network device isolation']),
('maintenance','Keep the server working','server-health','terminal',[
 'When adding packages later, copy them into the category folders and wait for the next five-minute scan. Then refresh the library in the PS4 app.',
 'To update the server image, use the pull and up commands in the companion notes, keeping the same project name. Your mounted covers, JSON, and packages remain in their folders.',
 'If you use sudo, include it with those commands. Keep the computer’s LAN address stable, for example with a router address reservation.'],
 ['Add files → scheduled scan → refresh on PS4','Updates: use the companion pull + up commands','Keep your computer’s LAN address stable']),
('recap','You are ready to use the library','server-library','browser',[
 'You now know the complete path: packages on the computer, Docker serving the library, the slash games source saved on PS4, and installation from Available.',
 'Use the companion commands and troubleshooting notes when you need a reminder. The video description includes the official project links and capture credits.',
 'PS4 Games Installer was created by e one z zero. Copyright twenty twenty-six, Vintage two thousand dot org. Thank you for watching.'],
 ['Server: healthy | Browser: packages visible','PS4: source tested, saved and refreshed','Created by e1z0 • Vintage2000.org • 2026']),
]
