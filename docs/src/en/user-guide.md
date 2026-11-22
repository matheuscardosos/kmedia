# User Guide

This guide covers the core KMedia workflow for preparing and presenting meeting media.

## Three-Column Responsive Operator Console

KMedia adapts automatically across desktop, split-screen booth laptops, and compact narrow panes (`360x520` minimum width for side-by-side Zoom use), and supports switching cleanly between **Studio Carbon (Dark)** and **Studio Daylight (Light)** themes (`Ctrl+D` or the sidebar toggle button):

1. **Wide (`>= 1080 px`):** Displays the full `234 px` left navigation rail with KMedia branding and congregation profile switcher, flexible center playlist workspace, and docked `344 px` Live Preview & Inspector card stack.
2. **Medium (`680 px` to `1079 px`):** Collapses the left rail to a narrow icon/badge bar (`64 px`) and places the Live Preview & Inspector into an on-demand slide-over drawer.
3. **Compact (`< 680 px`, down to `360x520`):** Moves navigation into a compact bottom tab bar (`54 px`), compacts the **Floating Pill Action Bar** (`ActionPillBar`) into an icon capsule, and keeps the full-width transport bar above navigation tabs.

![KMedia Desktop Workstation (Dark Theme)](../assets/kmedia-preview.png)

![KMedia Desktop Workstation (Light Theme)](../assets/kmedia-light-preview.png)

![KMedia Compact Side-by-Side Layout](../assets/kmedia-compact-preview.png)

![KMedia Searchable Two-Pane Settings](../assets/kmedia-settings-preview.png)

1. **Left Navigation Rail / Compact Tabs:** Switch congregations cleanly, toggle Dark/Light theme, move between the two pages, *Meeting* and *Settings*, each with the same header (icon, title, and congregation), and inspect live OBS Studio, 2nd Display, Zoom, and free disk space. The *Settings* page is only created the first time you open it.
2. **Meeting Media page (`MeetingMediaPage`):** The *Meeting* page opens on the first day from today on that has a meeting or media. Its header shows the meeting (or the date on other days) and the congregation, previous and next buttons that skip to the nearest day with a meeting or media, and a date button whose calendar marks each day of the sync window: green when ready, orange while syncing, red when something failed, and purple when the day holds media you added. Each section is a card with its colored icon, its name in the interface language and, for the *Watchtower Study*, the article title. Meeting sections always show on meeting days and other sections only when they hold media; on other days you can create, rename, recolor, reorder, and delete sections from their menus. Every item shows a 16:9 thumbnail with its duration (the picture itself, the preview picture JW.org publishes for its videos, or a frame of your own videos), its song or paragraph tag, and a play button; hiding, tags, renaming, slide intervals, moving, dividers, and deleting are in the item menu. Synced media can be hidden but not deleted, because the next sync would bring it back. **Add media** adds a song by number from JW.org, the media of one document of a JW.org publication (type its symbol and, for magazines and workbooks, the issue), the media of a public talk from your S-34mp (pick the `.jwpub` once, then search the talks by number or title; the talk theme becomes the section subtitle), or files from your computer to the section you pick (the *Public Talk* on a regular weekend); a `.jwpub` file, picked or dropped, opens the list of its documents so you add only the one you need, and the **Song** button of the *Public Talk* and *Circuit Overseer* sections adds their song while they have none. Drag an item by its handle onto another item to move it, across sections too. The page menu also holds **Special week**, **Sync this day again**, hidden media, **Reset order**, dividers, and the **Checklist** (`BoothChecklistPopup`).
3. **Meetings, Special Weeks & Sync:** *Settings > Meetings* finds your congregation on hub.jw.org, sets the meeting days and start times (also used for background music fade-outs), the media language and its fallback, and the media filters (footnotes, printed-only pictures, *Watchtower* paragraph videos, *Teaching* videos, Congregation Bible Study videos). **Special week** in the *Meeting* page menu marks the week of the open day as a Circuit Overseer visit (adds the *Circuit Overseer* section and moves the midweek meeting to Tuesday), the Memorial, a circuit assembly, or a regional convention (no regular meetings that week), or gives that week a temporary schedule. Meetings sync on their own: KMedia keeps the meetings of the next five weeks ready in the background (closest meeting first), each in its own day, and **Sync this day again** syncs the open day without losing the items you hid, renamed, moved, or added.
4. **Interactive Floating Pill Bar (`ActionPillBar`):** Features enlarged icon-first booth toggles with authentic OBS Studio and Zoom emblems, instant *Standby* and *Subtitles* (`libmpv`) toggles, status LEDs, and anchored quick popovers for Monitor Selection, Background Music controls, Meeting Timer (`Elapsed` / `Remaining` / `Overtime`), Live OBS Scene Switching & Recording, Zoom Launch, the download queue (every transfer with its progress, pause, resume, retry, and cancel, plus the concurrency), and Command Palette (`Ctrl+K`).
5. **Command Palette (`Ctrl+K`), Direct Shortcuts & Toast Feedback:** Press `Ctrl+K` anywhere or use direct operator shortcuts (`Space` play/pause, `B` standby, `Esc` stop playback, `Delete` remove the active item (synced media is hidden instead), `Ctrl+M` toggle/fade ambient music, `Alt+Left`/`Alt+Right` previous/next item, `Ctrl+P` toggle stage display, `Ctrl+D` switch theme, `Ctrl+O` add media, and `1` *Meeting* or `2` *Settings* when not editing text).
6. **Right Live Preview & Inspector Cards:** Monitor the 16:9 stage output with *ON AIR / READY / STANDBY* overlays, instant *Standby* / *Blackout* glass buttons, real file metadata, live routing status, non-destructive trim markers (`00:00.000`), and viewport zoom or PDF page controls.
7. **Searchable Two-Pane Settings & Booth PIN Lock:** Organize configuration cleanly across six searchable categories (*General*, *Meetings*, *Display*, *Audio*, *Integrations*, *Security*) with optional salted SHA-256 Booth PIN protection (`BoothSecurity` / `SettingsPinGate`) to prevent accidental changes on shared auditorium laptops, plus `--tab <meeting|settings>` launch targeting.

## Playback, Trim, Zoom, and PDF Presentation

- **Non-Destructive Video and Audio Trims:** Start (`trimStartMs`) and end (`trimEndMs`) markers are enforced directly by `libmpv` without modifying or re-encoding original files. Setting an end time earlier than or equal to the start time disables the upper bound cleanly.
- **Image and Video Viewport Controls:** Adjust zoom (`1.0x` to `8.0x`) and pan coordinates (`-0.5` to `+0.5`) smoothly across video, image, and PDF presentation output, or reset the viewport with one click.
- **Multi-Page PDF Documents:** Navigate pages (`Previous`, `Next`, or direct page selection) rendered natively through `Qt6::Pdf` at display resolution.

## Background Music and Meeting Schedules

- Background ambient music runs on a dedicated audio-only `libmpv` pipeline independent from presentation video playback.
- Configure midweek and weekend meeting start times so background music initiates an automatic smooth fade-out (default `5000 ms`) scheduled to finish right when the meeting starts. Manual pause or play overrides remain active for that session.

## Importing Media

- **Drag and Drop:** Drop video (`MP4`, `MKV`, `WebM`), audio (`MP3`, `AAC`, `OGG`, `FLAC`), image (`JPEG`, `PNG`, `WebP`), or `PDF` files directly onto the playlist.
- **Publication and Public Talk Packages (`.jwpub` and `S-34 / S-34mp`):** Open `.jwpub` archives natively with `minizip-ng` (including nested `contents` SQLite inspection and Zip-Slip path checks) and selectively extract only the requested meeting images, videos, or public talk media assets without cluttering disk storage.
- **JW Library Playlists (`.jwlplaylist`):** Import `.jwlplaylist` archives preserving `TagMap` item order, marker labels, loop flags (`EndAction`), embedded media/thumbnails, and non-destructive trim bounds converted cleanly from 100-nanosecond ticks.
- **Weekly Meeting Auto-Sync, Week Filters & Publication Catalog:** Switch seamlessly between date-keyed meeting days (**Midweek**, **Weekend**, **Circuit Overseer Visit**, **Memorial**, and custom non-meeting days) directly from the Meeting Workspace header strip or Calendar. KMedia resolves exact workbook dates via `.jwpub` `DatedText` and probes candidate Watchtower study articles (`6`, `8`, `10`, `12` weeks back), parses `Question` paragraph labels and `DocumentExtract` song/reference groups, resolves external videos and songs via JW Mediator (`v1/media-items`) with `GETPUBMEDIALINKS` and secondary language fallback, treats long files over `35` minutes (`2100 s`) as stream-only items, and provides Congregation Lookup (`hub.jw.org`) plus configurable extraction filters (footnotes, printed artwork, WT paragraph videos, `th`, CBS sources).
- **Meeting Organization Controls:** Group multi-item extracts inside collapsible groups, insert colored dividers, assign or click inline tags (`Song X`, `Para. Y`, `*`), toggle hidden items (`Hidden` filter toggle), adjust section repeat and slide intervals, reset original synced ordering, and filter playlist rows live (`Ctrl+F`).
- **Watched Cloud Folders:** Configure local Dropbox, OneDrive, Google Drive, or Nextcloud folders (or auto-detect standard sync folders) so newly added and settled items appear automatically with `500 ms` stability debouncing.

## Presenting on a Secondary Display

When a secondary monitor or projector is connected, KMedia automatically targets it for full-screen borderless presentation (`"KMedia - Presentation"`) while keeping the operator console on the primary screen. Use the *Standby / Blackout* control to switch smoothly between active media and a black screen or custom congregation standby image without losing playlist state.
