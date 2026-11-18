import { existsSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { defineConfig, type DefaultTheme, type HeadConfig } from 'vitepress';

import { messages, type LocaleMessages } from '../locales/index.ts';
import { BASE_PATH, CANONICAL_URL, GH_REPO_URL } from '../utils/constants.ts';

const currentDir = dirname(fileURLToPath(import.meta.url));
const publicDir = resolve(currentDir, '../src/public');

function buildOptionalHeadAssets(): HeadConfig[] {
  const entries: HeadConfig[] = [
    ['meta', { name: 'color-scheme', content: 'dark light' }],
    ['meta', { name: 'theme-color', content: '#FF6B00' }],
  ];

  if (existsSync(resolve(publicDir, 'favicon.ico'))) {
    entries.push(['link', { rel: 'icon', type: 'image/x-icon', href: `${BASE_PATH}favicon.ico` }]);
  }
  if (existsSync(resolve(publicDir, 'logo.svg'))) {
    entries.push(['link', { rel: 'icon', type: 'image/svg+xml', href: `${BASE_PATH}logo.svg` }]);
  }
  if (existsSync(resolve(publicDir, 'icons/icon-128x128.png'))) {
    entries.push([
      'link',
      { rel: 'icon', type: 'image/png', sizes: '128x128', href: `${BASE_PATH}icons/icon-128x128.png` },
    ]);
  }
  return entries;
}

function buildThemeConfig(prefix: string, msg: LocaleMessages): DefaultTheme.Config {
  return {
    logo: existsSync(resolve(publicDir, 'logo.svg')) ? '/logo.svg' : undefined,
    nav: [
      { text: msg.about, link: `${prefix}/about` },
      { text: msg.download, link: `${prefix}/download` },
      { text: msg.userGuide, link: `${prefix}/user-guide` },
      { text: msg.settingsGuide, link: `${prefix}/settings-guide` },
      { text: msg.faq, link: `${prefix}/faq` },
    ],
    sidebar: [
      {
        text: msg.title,
        items: [
          { text: msg.about, link: `${prefix}/about` },
          { text: msg.download, link: `${prefix}/download` },
          { text: msg.userGuide, link: `${prefix}/user-guide` },
          { text: msg.settingsGuide, link: `${prefix}/settings-guide` },
          { text: msg.faq, link: `${prefix}/faq` },
        ],
      },
    ],
    socialLinks: [{ icon: 'github', link: GH_REPO_URL }],
    footer: {
      message: msg.footerMessage,
      copyright: `Copyright © ${new Date().getFullYear()} Matheus Cardoso`,
    },
    editLink: {
      pattern: `${GH_REPO_URL}/edit/main/docs/src/:path`,
      text: msg.editLink,
    },
    outline: {
      label: msg.outline,
    },
    lastUpdated: {
      text: msg.lastUpdated,
    },
    docFooter: {
      prev: msg.docFooterPrev,
      next: msg.docFooterNext,
    },
    returnToTopLabel: msg.returnToTopLabel,
    sidebarMenuLabel: msg.sidebarMenuLabel,
    darkModeSwitchLabel: msg.darkModeSwitchLabel,
    langMenuLabel: msg.langMenuLabel,
  };
}

export default defineConfig({
  srcDir: './src',
  rewrites: {
    'en/:rest*': ':rest*',
  },
  base: BASE_PATH,
  cleanUrls: true,
  lastUpdated: true,
  appearance: 'dark',
  head: buildOptionalHeadAssets(),
  sitemap: {
    hostname: CANONICAL_URL,
  },
  locales: {
    root: {
      label: 'English',
      lang: 'en-US',
      title: messages.en.title,
      description: messages.en.description,
      themeConfig: buildThemeConfig('', messages.en),
    },
    pt: {
      label: 'Português',
      lang: 'pt-BR',
      link: '/pt/',
      title: messages.pt.title,
      description: messages.pt.description,
      themeConfig: buildThemeConfig('/pt', messages.pt),
    },
  },
  themeConfig: {
    search: {
      provider: 'local',
      options: {
        locales: {
          pt: {
            translations: {
              button: {
                buttonText: messages.pt.search,
                buttonAriaLabel: messages.pt.search,
              },
              modal: {
                noResultsText: messages.pt.noResultsText,
                resetButtonTitle: messages.pt.resetButtonTitle,
                backButtonTitle: messages.pt.backButtonTitle,
                footer: {
                  selectText: messages.pt.selectText,
                  navigateText: messages.pt.navigateText,
                  closeText: messages.pt.closeText,
                },
              },
            },
          },
        },
      },
    },
    externalLinkIcon: true,
  },
  transformPageData(pageData) {
    const isPortuguese = pageData.relativePath.startsWith('pt/');
    const prefix = isPortuguese ? '/pt' : '';
    const canonicalUrl = `${CANONICAL_URL}${pageData.relativePath
      .replace(/index\.md$/, '')
      .replace(/\.md$/, '')}`;

    pageData.frontmatter.head ??= [];
    pageData.frontmatter.head.push(
      ['link', { rel: 'canonical', href: canonicalUrl }],
      ['meta', { property: 'og:url', content: canonicalUrl }],
      ['meta', { property: 'og:site_name', content: 'KMedia' }],
      [
        'script',
        {},
        `{
          const visited = sessionStorage.getItem('kmediaLangChecked') === 'true';
          if (!visited && window.location.pathname === '${BASE_PATH}') {
            sessionStorage.setItem('kmediaLangChecked', 'true');
            const langs = navigator.languages || [navigator.language || ''];
            const prefersPt = langs.some((l) => l && l.toLowerCase().startsWith('pt'));
            if (prefersPt) {
              window.location.replace('${BASE_PATH}pt/');
            }
          }
        }`,
      ],
    );

    if (pageData.frontmatter.layout === 'home' && Array.isArray(pageData.frontmatter.hero?.actions)) {
      pageData.frontmatter.hero.actions = pageData.frontmatter.hero.actions.map(
        (action: { link: string; text: string; theme?: string }) => {
          const slug = action.link.replace(/^\/+(pt\/)?/, '');
          return { ...action, link: `${prefix}/${slug}` };
        },
      );
    }
  },
});
