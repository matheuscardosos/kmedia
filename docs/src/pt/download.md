---
title: Downloads
---

# Downloads

Baixe o KMedia para Windows, macOS ou Linux. Os instaladores prontos são publicados no GitHub Releases a cada marco concluído.

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

## Última Versão

- **Versão:** {{ downloads.version }} <span v-if="daysAgoText">({{ daysAgoText }})</span>
- **Página de Releases:** <a :href="downloads.releaseUrl" target="_blank" rel="noopener noreferrer">GitHub Releases</a>

<div v-if="!downloads.hasRelease" class="kmedia-download-card">
  <strong>Instaladores em preparação</strong>
  <p style="margin: 0.5rem 0 0;">{{ msg.noReleaseYet }}</p>
</div>

<div v-else-if="recommended" class="kmedia-download-card">
  <strong>{{ msg.recommendedFor }} {{ recommended.label }}</strong>
  <div>
    <a :href="recommended.href" class="kmedia-download-button">{{ msg.download }}</a>
  </div>
</div>

## Pacotes por Plataforma

### Windows

- **Instalador Windows 64-bit (.exe):** <a v-if="downloads.hasWinInstaller" :href="downloads.win64">Baixar</a><span v-else>Disponível a partir da próxima release (<a :href="downloads.releaseUrl" target="_blank" rel="noopener noreferrer">GitHub Releases</a>)</span> (instalação padrão por usuário sem exigir senha de administrador, com atalhos opcionais no Menu Iniciar e Área de Trabalho e desinstalador padrão do Windows)
- **Windows 64-bit Portátil (.zip):** <a :href="downloads.winPortable">Baixar</a>

### macOS

- **macOS Apple Silicon (.dmg):** <a :href="downloads.macDmg">Baixar</a>

### Linux

- **Linux x64 (.tar.gz):** <a :href="downloads.linuxTarGz">Baixar</a>

## Compilação pelo Código-Fonte

Para compilar a versão mais recente diretamente pelo código-fonte, siga as instruções de CMake e `vcpkg` no [repositório do GitHub](https://github.com/matheuscardosos/kmedia#building-from-source).
