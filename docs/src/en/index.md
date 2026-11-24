---
layout: home

hero:
  name: KMedia
  text: Native Congregation Media Presenter
  tagline: Built from scratch in C++20, Qt 6, and libmpv for silent operation, a single lightweight process, and hardware-accelerated dual-screen playback.
  image:
    src: /logo.svg
    alt: KMedia logo
  actions:
    - theme: brand
      text: Download
      link: /download
    - theme: alt
      text: About
      link: /about
    - theme: alt
      text: User Guide
      link: /user-guide
    - theme: alt
      text: Settings Guide
      link: /settings-guide
    - theme: alt
      text: FAQ
      link: /faq

features:
  - title: Lightweight Native Performance
    details: A single native process that opens in a few seconds, even on older laptops, and stays quiet on Kingdom Hall computers, with no bundled browser.
  - title: Hardware-Accelerated libmpv Engine
    details: Zero-copy OpenGL video rendering with automatic hardware decoding (hwdec=auto-safe), gapless audio, and non-destructive trim markers.
  - title: Automated Media Fetching
    details: Fetches weekly meeting media and catalogs across hundreds of languages with ETag caching and polite concurrency limits.
  - title: Native JWPUB, Playlist, and PDF Parsing
    details: Inspects .jwpub and .jwlplaylist archives natively in C++ and renders multi-page PDF documents through Qt6::Pdf.
  - title: Dual-Monitor Presentation
    details: Automatically tracks connected screens, assigns borderless output to secondary displays, and supports instant standby or blackout.
  - title: Live Preview and Camera Background
    details: Shows a playing video live in the preview panel or a floating preview, zooms and pans by hand, and puts a camera behind the presentation for sign language congregations.
  - title: Meeting Timer and Lectern Tablet
    details: Times each part of the meeting, shows how far ahead or behind it is, opens a timer window on a spare screen, and serves the timer to a tablet through a QR code.
  - title: Day by Day Meeting Page
    details: Opens each meeting day section by section, with real thumbnails and a calendar whose colored dots show which days are ready.
  - title: Songbook Background Music
    details: Plays the songbook in the media language before meetings, timed to end when the meeting starts, with the day's songs last.
  - title: Multi-Congregation Profiles
    details: Isolates SQLite WAL databases, storage paths, meeting schedules, and integration settings per congregation in under 100 ms.
  - title: OBS Studio & Zoom Integration
    details: Native OBS WebSocket v5 scene switching, lightweight process detection, and one-click Zoom launch.
  - title: Shared Folder and Media Export
    details: Brings media from dated Dropbox, OneDrive, Google Drive, or Nextcloud folders into each day, and exports each day in meeting order.
---
