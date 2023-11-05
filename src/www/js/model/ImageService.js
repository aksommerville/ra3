/* ImageService.js
 * Helper for decoding images.
 */
 
export class ImageService {
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
  }
  
  // Resolves to an Image object, which can be used as an <img> element.
  decodePng(bin) {
    const blob = new this.window.Blob([bin], { type: "image/png" });
    const url = this.window.URL.createObjectURL(blob);
    const element = this.window.document.createElement("IMG");
    element.src = url;
    return new Promise((resolve, reject) => {
      element.addEventListener("load", () => resolve(element));
      element.addEventListener("error", e => reject(e));
    });
  }
}

ImageService.singleton = true;
