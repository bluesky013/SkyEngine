import { resourceManager } from '@kit.LocalizationKit';

export const configure: (filesDir: string, resources: resourceManager.ResourceManager) => boolean;
export const setActive: (active: boolean) => boolean;
export const shutdown: () => boolean;
export const status: () => number;

export interface NativeTextInputCommand {
  show: boolean;
  token: string;
  text: string;
  placeholder: string;
  title: string;
  maxLength: number;
  multiline: boolean;
  password: boolean;
}
export const setTextInputHandler: (handler: ((command: NativeTextInputCommand) => void) | undefined) => boolean;
export const textInputEvent: (token: string, kind: number, text: string, nativeError: number) => boolean;
