# Frequently Asked Questions

## Why is KMedia written in C++20 and Qt 6 instead of Electron?

Electron bundles a full Chromium runtime and Node.js backend, which commonly uses 300 to 800+ MB of RAM and triggers CPU/fan spikes on low-power laptops. KMedia compiles to native machine code using Qt Quick and `libmpv`, targeting under 45 MB RAM at idle and GPU-accelerated playback.

## Can I manage multiple congregations on the same computer?

Yes. Each congregation profile has its own SQLite WAL database, media directory, schedule, and OBS/Zoom configuration. Switching profiles updates the active workspace cleanly without restarting the app.

## Does KMedia re-encode videos when trimming start or end times?

No. Start and end trim timestamps are stored as metadata and enforced directly by `libmpv` during playback, preserving original video quality and saving CPU/disk time.

## How do I report a bug or request a feature?

Open an issue on the [GitHub Issues page](https://github.com/matheuscardosos/kmedia/issues) with your operating system, KMedia version, and steps to reproduce.
