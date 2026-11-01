import { resourceManager } from '@kit.LocalizationKit';

export const configure: (filesDir: string, resources: resourceManager.ResourceManager) => boolean;
export const ready: () => boolean;
export const run: () => string;
export const startStress: () => boolean;
export const stressStats: () => string;
export const stopStress: () => string;
export const setActive: (active: boolean) => boolean;
