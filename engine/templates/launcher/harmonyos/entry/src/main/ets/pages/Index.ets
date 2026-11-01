import { hilog } from '@kit.PerformanceAnalysisKit';
import engine from 'lib{{LAUNCHER_LIB_NAME}}.so';
import { NativeTextInputCommand } from 'lib{{LAUNCHER_LIB_NAME}}.so';
import { NativeTextInputDialog } from '../components/NativeTextInputDialog';

@Entry
@Component
struct Index {
  @StorageLink('engineReady') ready: boolean = false;
  @State failed: boolean = false;
  @State stopped: boolean = false;
  private timer: number = -1;
  private inputDialog: CustomDialogController | undefined = undefined;
  private inputToken: string = '';
  private inputText: string = '';

  aboutToAppear(): void {
    if (!engine.setTextInputHandler((command: NativeTextInputCommand) => this.handleTextInput(command))) {
      hilog.error(0, 'SkyEngine', 'System text input UI registration failed');
    }
    this.timer = setInterval(() => {
      const status = engine.status();
      this.failed = status === 3;
      this.stopped = status === 4;
    }, 250);
  }

  aboutToDisappear(): void {
    this.finishTextInput(this.inputToken, 2, this.inputText);
    engine.setTextInputHandler(undefined);
    clearInterval(this.timer);
  }

  private finishTextInput(token: string, kind: number, text: string): void {
    if (token.length === 0 || token !== this.inputToken) {
      return;
    }
    const controller = this.inputDialog;
    this.inputDialog = undefined;
    this.inputToken = '';
    this.inputText = '';
    controller?.close();
    engine.textInputEvent(token, kind, text, 0);
  }

  private handleTextInput(command: NativeTextInputCommand): void {
    if (!command.show) {
      if (command.token === this.inputToken) {
        this.finishTextInput(command.token, 2, this.inputText);
      }
      return;
    }
    this.finishTextInput(this.inputToken, 2, this.inputText);
    const token = command.token;
    this.inputToken = token;
    this.inputText = command.text;
    this.inputDialog = new CustomDialogController({
      builder: NativeTextInputDialog({ command: command,
        onChanged: (value: string) => {
          if (this.inputToken === token) {
            this.inputText = value;
            if (!engine.textInputEvent(token, 0, value, 0)) {
              this.finishTextInput(token, 2, this.inputText);
            }
          }
        },
        onConfirmed: (value: string) => this.finishTextInput(token, 1, value),
        onCancelled: (value: string) => this.finishTextInput(token, 2, value)
      }),
      autoCancel: true,
      cancel: () => this.finishTextInput(token, 2, this.inputText)
    });
    this.inputDialog.open();
  }

  build() {
    Stack() {
      if (this.ready) {
        XComponent({ id: 'SkyEngineSurface', type: XComponentType.SURFACE, libraryname: '{{LAUNCHER_LIB_NAME}}' })
          .width('100%')
          .height('100%')
          .focusable(true)
          .defaultFocus(true)
          .focusOnTouch(true)
      }
      if (!this.ready || this.failed || this.stopped) {
        Text(this.stopped ? 'Engine stopped. See device logs.' : 'Engine initialization failed. See device logs.')
          .fontColor(Color.White)
          .padding(24)
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#101820')
  }
}
