import { defineLoader } from 'vitepress';

import { fetchLatestRelease } from '../utils/api.ts';

export interface Data {
  version: string;
  publishedAt: string;
  releaseUrl: string;
  hasRelease: boolean;
  win64: string;
  winPortable: string;
  macUniversal: string;
  linuxAppImage: string;
}

declare const data: Data;
export { data };

export default defineLoader({
  async load(): Promise<Data> {
    const release = await fetchLatestRelease();
    const findAsset = (suffix: string, fallback: string) =>
      release.assets.find((asset) => asset.name.endsWith(suffix))?.browser_download_url ?? fallback;

    const hasRelease = release.assets.length > 0;
    const baseUrl = release.releaseUrl;

    return {
      version: release.tag,
      publishedAt: release.publishedAt,
      releaseUrl: release.releaseUrl,
      hasRelease,
      win64: findAsset('-win64-setup.exe', baseUrl),
      winPortable: findAsset('-win64-portable.zip', baseUrl),
      macUniversal: findAsset('-macos-universal.dmg', baseUrl),
      linuxAppImage: findAsset('-linux-x86_64.AppImage', baseUrl),
    };
  },
});
