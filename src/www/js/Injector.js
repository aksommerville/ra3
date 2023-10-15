/* Injector.js
 */
 
export class Injector {
  static getDependencies() {
    return [Window, Document];
  }
  constructor(window, document) {
    this.singletons = {
      Window: window,
      Document: document,
      Injector: this,
    };
    this.inProgress = [];
    this.nextDiscriminator = 1;
  }
  
  get(clazz, overrides) {
    if (clazz === "discriminator") return this.nextDiscriminator++;
    if (overrides) {
      let instance = overrides.find(o => o instanceof clazz);
      if (instance) return instance;
    }
    let instance = this.singletons[clazz.name];
    if (instance) return instance;
    if (this.inProgress.includes(clazz.name)) {
      throw new Error(`Dependency cycle: ${JSON.stringify(this.inProgress)}`);
    }
    this.inProgress.push(clazz.name);
    const depClasses = clazz.getDependencies?.() || [];
    const deps = depClasses.map(depClass => this.get(depClass, overrides));
    instance = new clazz(...deps);
    const p = this.inProgress.indexOf(clazz.name);
    if (p >= 0) this.inProgress.splice(p, 1);
    if (clazz.singleton) this.singletons[clazz.name] = instance;
    return instance;
  }
  
}

Injector.singleton = true;
