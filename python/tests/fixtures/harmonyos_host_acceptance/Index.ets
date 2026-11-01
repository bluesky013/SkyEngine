import acceptance from 'libOHOSHostAcceptance.so';

@Entry
@Component
struct Index {
  @State report: string = 'Waiting for two SDK surfaces';
  @State stressRunning: boolean = false;
  @State showSurface: boolean = false;
  private timer: number = -1;
  private stressTimer: number = -1;
  private stopTimer: number = -1;
  private attempts: number = 0;

  aboutToAppear(): void {
    this.timer = setInterval(() => {
      if (acceptance.ready()) {
        clearInterval(this.timer);
        this.report = acceptance.run();
        console.info('OHOSHostAcceptance ' + this.report);
      } else if (++this.attempts > 100) {
        clearInterval(this.timer);
        this.report = '{"failed":1,"reason":"real surfaces unavailable"}';
        console.error('OHOSHostAcceptance ' + this.report);
      }
    }, 100);
  }

  aboutToDisappear(): void {
    clearInterval(this.timer);
    clearInterval(this.stressTimer);
    clearTimeout(this.stopTimer);
    if (this.stressRunning) {
      acceptance.stopStress();
    }
  }

  private beginStress(): void {
    if (this.stressRunning || !acceptance.startStress()) {
      return;
    }
    this.stressRunning = true;
    this.showSurface = true;
    this.stressTimer = setInterval(() => {
      this.showSurface = !this.showSurface;
      this.report = acceptance.stressStats();
    }, 600);
    this.stopTimer = setTimeout(() => {
      clearInterval(this.stressTimer);
      this.report = acceptance.stopStress();
      this.stressRunning = false;
      console.info('OHOSHostAcceptance STRESS ' + this.report);
    }, 65000);
  }

  build() {
    Column() {
      Text(this.report).fontSize(18).padding(16)
      Button('Run 65s surface stress').onClick(() => this.beginStress())
      if (this.stressRunning) {
        if (this.showSurface) {
          XComponent({ id: 'HostStress', type: XComponentType.SURFACE, libraryname: 'OHOSHostAcceptance' })
            .width('90%').height(200)
        }
      } else {
        Row() {
          XComponent({ id: 'HostAcceptanceFirst', type: XComponentType.SURFACE, libraryname: 'OHOSHostAcceptance' })
            .width('45%').height(200)
          XComponent({ id: 'HostAcceptanceSecond', type: XComponentType.SURFACE, libraryname: 'OHOSHostAcceptance' })
            .width('45%').height(200)
        }
      }
    }
    .width('100%').height('100%')
  }
}
