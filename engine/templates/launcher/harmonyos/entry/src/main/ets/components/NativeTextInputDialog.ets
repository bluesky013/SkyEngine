import { NativeTextInputCommand } from 'lib{{LAUNCHER_LIB_NAME}}.so';

@CustomDialog
export struct NativeTextInputDialog {
  controller?: CustomDialogController;
  command: NativeTextInputCommand = {
    show: false, token: '', text: '', placeholder: '', title: '',
    maxLength: 0, multiline: false, password: false
  };
  onChanged: (value: string) => void = () => {};
  onConfirmed: (value: string) => void = () => {};
  onCancelled: (value: string) => void = () => {};
  @State private value: string = '';
  private committed: string = '';

  aboutToAppear(): void {
    this.value = this.command.text;
    this.committed = this.command.text;
  }

  private limit(value: string): string {
    if (this.command.maxLength === 0) {
      return value;
    }
    let offset = 0;
    let count = 0;
    while (offset < value.length && count < this.command.maxLength) {
      const unit = value.charCodeAt(offset++);
      if (unit >= 0xD800 && unit <= 0xDBFF && offset < value.length) {
        const next = value.charCodeAt(offset);
        if (next >= 0xDC00 && next <= 0xDFFF) {
          offset++;
        }
      }
      count++;
    }
    return value.substring(0, offset);
  }

  private edit(value: string, preview: PreviewText | undefined): void {
    this.value = value;
    if (preview !== undefined && preview.value.length > 0) {
      return;
    }
    this.value = this.limit(value);
    this.committed = this.value;
    this.onChanged(this.committed);
  }

  aboutToDisappear(): void {
    this.value = '';
    this.committed = '';
    this.command.text = '';
  }

  build() {
    Column({ space: 8 }) {
      if (this.command.title.length > 0) {
        Text(this.command.title).maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
      }
      Row({ space: 8 }) {
        Scroll() {
          if (this.command.multiline) {
            TextArea({ text: this.value, placeholder: this.command.placeholder })
              .id('sky-native-text-input')
              .height(180)
              .defaultFocus(true)
              .onChange((value: string, preview?: PreviewText) => this.edit(value, preview))
          } else {
            TextInput({ text: this.value, placeholder: this.command.placeholder })
              .id('sky-native-text-input')
              .type(this.command.password ? InputType.Password : InputType.Normal)
              .enterKeyType(EnterKeyType.Done)
              .defaultFocus(true)
              .onChange((value: string, preview?: PreviewText) => this.edit(value, preview))
              .onSubmit(() => this.onConfirmed(this.limit(this.value)))
          }
        }
        .layoutWeight(1)
        .height('100%')
        Button($r('app.string.sky_input_cancel'))
          .id('sky-native-text-cancel')
          .onClick(() => this.onCancelled(this.committed))
        Button($r('app.string.sky_input_confirm'))
          .id('sky-native-text-confirm')
          .onClick(() => this.onConfirmed(this.limit(this.value)))
      }
      .layoutWeight(1)
    }
    .padding(12)
    .height('100%')
    .width('90%')
  }
}
