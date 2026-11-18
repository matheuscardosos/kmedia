import { GH_OWNER, GH_REPO, GH_RELEASES_URL } from './constants.ts';

export interface ReleaseAssetInfo {
  name: string;
  browser_download_url: string;
}

export interface LatestReleaseInfo {
  tag: string;
  publishedAt: string;
  releaseUrl: string;
  assets: ReleaseAssetInfo[];
}

const FALLBACK_RELEASE: LatestReleaseInfo = {
  tag: 'v0.1.0',
  publishedAt: '',
  releaseUrl: GH_RELEASES_URL,
  assets: [],
};

export async function fetchLatestRelease(): Promise<LatestReleaseInfo> {
  try {
    const response = await fetch(
      `https://api.github.com/repos/${GH_OWNER}/${GH_REPO}/releases/latest`,
      {
        headers: {
          Accept: 'application/vnd.github+json',
          'User-Agent': `${GH_REPO}-docs-builder`,
        },
      },
    );

    if (!response.ok) {
      return FALLBACK_RELEASE;
    }

    const payload = (await response.json()) as {
      tag_name?: string;
      published_at?: string;
      html_url?: string;
      assets?: ReleaseAssetInfo[];
    };

    return {
      tag: payload.tag_name ?? FALLBACK_RELEASE.tag,
      publishedAt: payload.published_at ?? '',
      releaseUrl: payload.html_url ?? GH_RELEASES_URL,
      assets: Array.isArray(payload.assets) ? payload.assets : [],
    };
  } catch {
    return FALLBACK_RELEASE;
  }
}
