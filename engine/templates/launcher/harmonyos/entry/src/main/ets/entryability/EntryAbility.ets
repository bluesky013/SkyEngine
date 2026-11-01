import { UIAbility } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { BusinessError } from '@kit.BasicServicesKit';
import engine from 'lib{{LAUNCHER_LIB_NAME}}.so';

export default class EntryAbility extends UIAbility {
  private windowStage: window.WindowStage | undefined = undefined;

  onCreate(): void {
    const ready = engine.configure(this.context.filesDir, this.context.resourceManager);
    AppStorage.setOrCreate('engineReady', ready);
    if (!ready) {
      hilog.error(0, 'SkyEngine', 'Native host configuration failed');
    }
  }

  onWindowStageCreate(stage: window.WindowStage): void {
    this.windowStage = stage;
    stage.getMainWindow().then((mainWindow: window.Window) => {
      return mainWindow.setWindowLayoutFullScreen(true)
        .then(() => mainWindow.setWindowSystemBarEnable([]))
        .then(() => mainWindow.setSpecificSystemBarEnabled('navigationIndicator', false));
    }).then(() => {
      if (this.windowStage === stage) {
        this.loadGamePage(stage);
      }
    }, (error: BusinessError) => {
      if (this.windowStage === stage) {
        hilog.error(0, 'SkyEngine', 'Fullscreen configuration failed: %{public}d', error.code);
        AppStorage.setOrCreate('engineReady', false);
        this.loadGamePage(stage);
      }
    });
  }

  onWindowStageDestroy(): void {
    this.windowStage = undefined;
  }

  private loadGamePage(stage: window.WindowStage): void {
    stage.loadContent('pages/Index', (error) => {
      if (error.code) {
        hilog.error(0, 'SkyEngine', 'Page load failed: %{public}d', error.code);
      }
    });
  }

  onForeground(): void {
    if (!engine.setActive(true)) {
      hilog.error(0, 'SkyEngine', 'Foreground transition rejected');
    }
  }

  onBackground(): void {
    if (!engine.setActive(false)) {
      hilog.error(0, 'SkyEngine', 'Background transition rejected');
    }
  }

  onDestroy(): void {
    if (!engine.shutdown()) {
      hilog.error(0, 'SkyEngine', 'Native host shutdown failed');
    }
  }
}
