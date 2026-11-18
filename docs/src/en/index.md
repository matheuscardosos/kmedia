---
layout: home

hero:
  name: KMedia
  text: Native Congregation Media Presenter
  tagline: Built from scratch in C++20, Qt 6, and libmpv for silent operation, sub-45 MB idle RAM, and hardware-accelerated dual-screen playback.
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
    details: Designed for under 45 MB idle RAM, sub-400 ms cold startup, and silent thermal behavior on Kingdom Hall laptops.
  - title: Hardware-Accelerated libmpv Engine
    details: Zero-copy OpenGL video rendering with automatic hardware decoding (hwdec=auto-safe), gapless audio, and non-destructive trim markers.
  - title: Automated Media Fetching
    details: Fetches weekly meeting media and catalogs across hundreds of languages with ETag caching and polite concurrency limits.
  - title: Native JWPUB, Playlist, and PDF Parsing
    details: Inspects .jwpub and .jwlplaylist archives natively in C++ and renders multi-page PDF documents through Qt6::Pdf.
  - title: Dual-Monitor Presentation
    details: Automatically tracks connected screens, assigns borderless output to secondary displays, and supports instant standby or blackout.
  - title: Scheduled Background Music
    details: Plays ambient pre-meeting and post-meeting music on an isolated audio pipeline with automatic 5-second fade-out before meeting start.
  - title: Multi-Congregation Profiles
    details: Isolates SQLite WAL databases, storage paths, meeting schedules, and integration settings per congregation in under 100 ms.
  - title: OBS Studio & Zoom Integration
    details: Native OBS WebSocket v5 scene switching, lightweight process detection, dedicated audio device routing, and one-click Zoom launch.
  - title: Cloud Folder Synchronization
    details: Watches Dropbox, OneDrive, Google Drive, and Nextcloud directories via OS filesystem notifications without disk polling.
---
