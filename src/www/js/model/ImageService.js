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
  
  isPng(bin) {
    if (!bin?.byteLength || (bin.byteLength < 8)) return false;
    const u8 = new Uint8Array(bin);
    if (u8[0] !== 0x89) return false;
    if (u8[1] !== 0x50) return false;
    if (u8[2] !== 0x4e) return false;
    if (u8[3] !== 0x47) return false;
    if (u8[4] !== 0x0d) return false;
    if (u8[5] !== 0x0a) return false;
    if (u8[6] !== 0x1a) return false;
    if (u8[7] !== 0x0a) return false;
    return true;
  }
}

ImageService.singleton = true;
