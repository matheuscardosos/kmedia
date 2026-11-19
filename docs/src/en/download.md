---
title: Downloads
---

# Downloads

Download KMedia for Windows, macOS, or Linux. Prebuilt packages are published through GitHub Releases as each milestone ships.

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import { useData } from 'vitepress';
import { data as downloads } from '../../data/version.data.mts';
import { messages } from '../../locales/index.ts';

const { lang } = useData();
const msg = computed(() => (lang.value.startsWith('pt') ? messages.pt : messages.en));

const daysAgoText = computed(() => {
  if (!downloads.publishedAt) return '';
  const released = new Date(downloads.publishedAt).getTime();
  const days = Math.floor((Date.now() - released) / (1000 * 60 * 60 * 24));
  if (Number.isNaN(days) || days < 0) return '';
  if (days === 0) return msg.value.releasedToday;
  if (days === 1) return msg.value.released1DayAgo;
  return msg.value.releasedXDaysAgo.replace('{days}', days.toString());
});

interface RecommendedTarget {
  href: string;
  label: string;
}

const recommended = ref<RecommendedTarget | null>(null);

onMounted(() => {
  if (!downloads.hasRelease) {
    recommended.value = null;
    return;
  }

  const ua = navigator.userAgent || '';
  if (/Android|iPhone|iPad|iPod/i.test(ua)) {
    recommended.value = null;
    return;
  }

  if (/Windows/i.test(ua)) {
    recommended.value = { href: downloads.win64, label: msg.value.windows64Bit };
  } else if (/Macintosh|Mac OS X/i.test(ua)) {
    recommended.value = { href: downloads.macDmg, label: msg.value.macDmg };
  } else if (/Linux/i.test(ua)) {
    recommended.value = { href: downloads.linuxTarGz, label: msg.value.linuxTarGz };
  }
});
</script>

## Latest Release

- **Version:** {{ downloads.version }} <span v-if="daysAgoText">({{ daysAgoText }})</span>
- **Releases Page:** <a :href="downloads.releaseUrl" target="_blank" rel="noopener noreferrer">GitHub Releases</a>

<div v-if="!downloads.hasRelease" class="kmedia-download-card">
  <strong>Prebuilt packages pending</strong>
  <p style="margin: 0.5rem 0 0;">{{ msg.noReleaseYet }}</p>
</div>

<div v-else-if="recommended" class="kmedia-download-card">
  <strong>{{ msg.recommendedFor }} {{ recommended.label }}</strong>
  <div>
    <a :href="recommended.href" class="kmedia-download-button">{{ msg.download }}</a>
  </div>
</div>

## Platform Packages

### Windows

- **Windows 64-bit Installer (.exe):** <a v-if="downloads.hasWinInstaller" :href="downloads.win64">Download</a><span v-else>Available starting with the next release (<a :href="downloads.releaseUrl" target="_blank" rel="noopener noreferrer">GitHub Releases</a>)</span> (installs per-user cleanly without administrator rights by default, with optional Start Menu and Desktop shortcuts and full Windows uninstaller support)
- **Windows 64-bit Portable (.zip):** <a :href="downloads.winPortable">Download</a>

### macOS

- **macOS Apple Silicon (.dmg):** <a :href="downloads.macDmg">Download</a>

### Linux

- **Linux x64 (.tar.gz):** <a :href="downloads.linuxTarGz">Download</a>

## Building from Source

If you want to build the latest development snapshot from source, follow the CMake and `vcpkg` instructions in the [GitHub repository](https://github.com/matheuscardosos/kmedia#building-from-source).
