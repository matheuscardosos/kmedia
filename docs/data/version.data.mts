import { defineLoader } from 'vitepress';

import { fetchLatestRelease } from '../utils/api.ts';

export interface Data {
  version: string;
  publishedAt: string;
  releaseUrl: string;
  hasRelease: boolean;
  hasWinInstaller: boolean;
  win64: string;
  winPortable: string;
  macDmg: string;
  linuxTarGz: string;
}

declare const data: Data;
export { data };

export default defineLoader({
  async load(): Promise<Data> {
    const release = await fetchLatestRelease();
    const findAsset = (suffixes: string[], fallback: string) => {
      for (const suffix of suffixes) {
        const match = release.assets.find((asset) => asset.name.endsWith(suffix));
        if (match) {
          return match.browser_download_url;
        }
      }
      return fallback;
    };

    const hasRelease = release.assets.length > 0;
    const baseUrl = release.releaseUrl;
    const winPortable = findAsset(['-windows-x64.zip', '-win64-portable.zip'], baseUrl);
    const winInstaller = findAsset(['-windows-x64-setup.exe', '-win64-setup.exe'], '');

    return {
      version: release.tag,
      publishedAt: release.publishedAt,
      releaseUrl: release.releaseUrl,
      hasRelease,
      hasWinInstaller: winInstaller.length > 0,
      win64: winInstaller || winPortable,
      winPortable,
      macDmg: findAsset(['-macos-arm64.dmg', '-macos-universal.dmg'], baseUrl),
      linuxTarGz: findAsset(['-linux-x64.tar.gz', '-linux-x86_64.AppImage'], baseUrl),
    };
  },
});
