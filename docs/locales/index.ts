import en from './en.json' with { type: 'json' };
import pt from './pt.json' with { type: 'json' };

export type LocaleMessages = typeof en;

export const messages = {
  en,
  pt,
} as const;

export const enabledLocales = ['pt'] as const;
