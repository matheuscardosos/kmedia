# About KMedia

**KMedia** is an open-source, cross-platform desktop application written in **C++20**, **Qt 6**, and **libmpv** to download, organize, and present audio, video, images, and PDF documents for congregation meetings.

## Why I Built KMedia

Browser-wrapped desktop presenters consume hundreds of megabytes of RAM, spawn multiple helper processes, and drive high CPU and fan noise on older Kingdom Hall laptops. I engineered KMedia from scratch to deliver full meeting media management with minimal resource usage:

- **Low Memory Footprint:** Well under 200 MB of private memory once started on Windows, with a single process and no bundled browser.
- **Cold & Quiet Hardware:** Uses `libmpv` GPU hardware decoding (`hwdec=auto-safe`) and event-driven OS notifications instead of polling loops.
- **Fast Startup:** The window is ready in a few seconds, even on older laptops, with SQLite3 in WAL mode and disk-backed thumbnail caching.
- **Studio Carbon & Broadcast Orange UI:** Tuned for low eye strain and zero glare in dimly lit auditoriums.

## Core Capabilities

- **Weekly Meeting Media:** Automatically organizes meeting images, videos, songs, and publications by date and language.
- **Archive & Document Support:** Imports `.jwpub`, `.jwlplaylist`, `S-34` / `S-34mp` public talk packages, local video/audio/images, and multi-page PDFs.
- **Precision Operator Controls:** Non-destructive start/end trim markers, synchronized zoom/pan viewports, drag-and-drop playlist ordering, and instant standby blackout.
- **Meeting Timer:** Part by part timing with ahead or behind tracking, a timer window for a spare screen, a read only page for a lectern tablet opened by QR code, and PDF or CSV timing reports.
- **Hybrid Booth Integrations:** Native OBS Studio WebSocket v5 control, Zoom launch (`zoommtg://`), and lightweight OS process detection.
- **Multilingual Interface:** Interface translations managed via Qt Linguist independently from the downloaded meeting media language.

## Inspiration

The day by day meeting workflow of KMedia is inspired by [Meeting Media Manager](https://github.com/sircharlo/meeting-media-manager), the open source app that many congregations already use. KMedia is an independent native app written from scratch, and it is not meant to compete with it.
