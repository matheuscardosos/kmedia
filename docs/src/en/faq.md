# Frequently Asked Questions

## General

### Where does KMedia get the meeting media?

Directly from the official website of Jehovah's Witnesses and its content delivery network, the same publications and files that JW Library uses. KMedia works out what each meeting needs and downloads it, checking every file against the checksum published with it. It does not use other sites, packages, or curators. The source code is open, so anyone can check what it does.

### Does KMedia respect the Terms of Use of jw.org?

Yes. The [Terms of Use](https://www.jw.org/finder?docid=1011511&prefer=content) of jw.org allow free, non-commercial applications that download electronic files such as EPUB, PDF, MP3, and MP4 from public areas of the site. KMedia is free and non-commercial, and it only downloads files like these from public areas.

### Which systems does KMedia run on?

Windows 10 or later (64-bit), macOS on Apple silicon, and Linux on x64. Get the right package from the [download page](./download.md).

### Does KMedia work in my language?

The meeting media comes in any language published on jw.org, with a fallback language for anything missing. The interface is available in English and Portuguese, and it is set apart from the media language, so each can be different. Sign language congregations get their songs from the sign language songbook automatically.

## Meeting media

### How does KMedia know what each meeting needs?

For the midweek meeting it reads the meeting workbook, and for the weekend meeting it finds the *Watchtower* study article of that week. The pictures, videos, and songs go into their sections in paragraph order. The [User Guide](./user-guide.md#how-meetings-sync) explains how syncing works.

### How far ahead does KMedia prepare meetings?

Five weeks, starting with the current one, closest meeting first. Each new week is added as the calendar moves on. Days with a meeting or with media you added are marked in the calendar of the *Meeting* page.

### How do I add my own media?

Use **Add media** on the *Meeting* page, or drop files on the window. KMedia takes videos, audio, pictures, PDF documents, `.jwpub` publications, and `.jwlplaylist` playlists from JW Library. Songs, JW.org videos, publication media, and public talk media have their own pickers in the same menu.

### Does trimming a video change the file?

No. The start and end you set are saved apart from the file and applied while the video plays, so the original keeps its quality and nothing is converted.

## Presenting

### How do I present on a second display or projector?

Connect it before or after opening KMedia. KMedia presents on it full screen and keeps the controls on the main screen. Pick another screen in *Settings > Display*, and show or hide the presentation window with `Ctrl+P`.

### Which keyboard shortcuts are there?

The [User Guide](./user-guide.md#keyboard-shortcuts) lists them all. The main ones are `Space` to play or pause, `B` for standby, and `Alt+Left` or `Alt+Right` for the previous or next item.

### How does background music work?

It plays the songbook in the media language: before a meeting of the same day it is timed to end when the meeting starts, with that meeting's songs last, and at other times it loops the whole songbook. It can start by itself before meetings; see the [User Guide](./user-guide.md#background-music).

### How do I time the parts of the meeting?

Open the clock pill. It lists the parts of the open day with their usual minutes; **Start** times the next part and **Next part** moves on. It shows how far ahead or behind the meeting is, keeps the real times with the day, and saves a timing report as PDF or CSV. See the [User Guide](./user-guide.md#meeting-timer).

### Can the speaker see the time on a tablet at the lectern?

Yes. Under **Options** in the timer, turn on **Page for a tablet**, then scan the QR code with a tablet or phone on the same network. The page only shows the timer, and only devices given its address, which carries an access code, can open it. **Timer window** shows the same timer on a spare screen instead.

## Integrations

### How do I use KMedia with Zoom?

Save the meeting ID, the passcode, and the name to join with in *Settings > Integrations*. Then join from **Launch Zoom Meeting** there or from the Zoom button of the pill bar, and share the presentation display in Zoom as usual.

### How do I connect OBS Studio?

In OBS Studio, turn on the WebSocket server in *Tools > WebSocket Server Settings* and note its port and password. In KMedia, turn on OBS Studio in *Settings > Integrations*, enter the host, port, and password, and click **Save & Connect**. Then pick the stage and media scenes, so KMedia switches to the media scene while media is presented and back to the stage afterward. The [Settings Guide](./settings-guide.md#obs-studio) explains each scene.

### How does the shared folder work?

Pick a folder that Dropbox, OneDrive, Google Drive, or Nextcloud keeps in sync, on every computer that presents. Media placed in its date folders (`YYYY-MM-DD`) shows up in that day on all of them. The [Settings Guide](./settings-guide.md#shared-folder) explains where each file lands.

### What is media export for?

For a computer or player that only needs the files. KMedia copies every day with media into a date folder, in meeting order and with numbered names, and keeps the copy up to date. See the [Settings Guide](./settings-guide.md#media-export).

### Can I manage several congregations on the same computer?

Yes. Each congregation profile has its own settings, media, schedule, and integrations. Switch from the congregation selector at the top of the navigation, without restarting.

## Troubleshooting

### Media is not downloading. What should I check?

- Check the internet connection.
- A red dot in the calendar marks a day with media that failed. Open that day and click **Try again**.
- The downloads button of the pill bar shows what is in progress and what failed.
- If an item does not exist in the media language, set a **Fallback language** in *Settings > Meetings*.

### OBS Studio does not connect. What should I check?

- OBS Studio is open and its WebSocket server is on.
- The host, port, and password in *Settings > Integrations* match the ones in *Tools > WebSocket Server Settings* of OBS Studio (`127.0.0.1` and `4455` on the same computer).
- Click **Save & Connect** again after changing them.
- If it connects but never switches scenes, pick the stage and media scenes in *Settings > Integrations*.

### The tablet does not open the timer page. What should I check?

- The tablet and this computer are on the same network (the same Wi-Fi, not a guest network).
- The address or QR code is the current one: **New code** makes the old address stop working.
- No message under **Page for a tablet** says the port is in use; if it does, choose another port.
- The firewall lets KMedia use the network. On Windows, allow it on private networks when asked, or in *Windows Security > Firewall*.

### Shared folder media does not show up. What should I check?

- The files are inside a date folder (`YYYY-MM-DD`) placed directly in the shared folder.
- The cloud app finished syncing on this computer.
- The shared folder is not also the media export folder.

## Technical questions

### Why is KMedia written in C++ and Qt instead of Electron?

Apps built on Electron carry a whole web browser with them. KMedia is a native app built with Qt and libmpv, so it starts fast, uses much less memory, and plays video on the graphics card, which keeps older Kingdom Hall laptops quiet and cool.

### Does KMedia collect data about me?

No. KMedia has no telemetry and sends no reports. It only connects to the servers of jw.org, to download media and look up congregations, and to OBS Studio and Zoom on your own computer when you use them. If you turn on the timer's **Page for a tablet**, it also serves that read only page on your local network. Your settings and media stay on your computer.

### Does KMedia update itself?

Not yet. New versions are published on the [download page](./download.md) and on GitHub Releases.

## Support

### How do I report a problem or ask for a feature?

Open an issue on [GitHub](https://github.com/matheuscardosos/kmedia/issues). For a problem, include the diagnostics line from *Settings > Security* and the steps that lead to it.

### How can I help?

Code, translations, and documentation are all welcome. [CONTRIBUTING.md](https://github.com/matheuscardosos/kmedia/blob/main/CONTRIBUTING.md) explains how to build KMedia and send changes.
