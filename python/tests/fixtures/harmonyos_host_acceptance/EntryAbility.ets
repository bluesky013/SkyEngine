import { UIAbility } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import acceptance from 'libOHOSHostAcceptance.so';

export default class EntryAbility extends UIAbility {
  onCreate(): void {
    if (!acceptance.configure(this.context.filesDir, this.context.resourceManager)) {
      console.error('OHOSHostAcceptance ResourceManager configuration failed');
    }
  }

  onWindowStageCreate(stage: window.WindowStage): void {
    stage.loadContent('pages/Index', (error) => {
      if (error.code) {
        console.error('OHOSHostAcceptance page load failed ' + error.code);
      }
    });
  }

  onForeground(): void {
    acceptance.setActive(true);
  }

  onBackground(): void {
    acceptance.setActive(false);
  }
}
