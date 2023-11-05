/* StateService.js
 * Manages the persistent application state.
 * This can be expressed as a dictionary of scalars,
 * or encoded to a private format with the base64 alphabet.
 * Trailing spaces in string values get trimmed (we kind of have to).
 */
 
export class StateService {
  static getDependencies() {
    return [];
  }
  constructor() {
    this.textDecoder = new TextDecoder("utf-8");
    this.textEncoder = new TextEncoder("utf-8");
    this.state = this.newBlankState();
    this.listeners = []; // { id, cb, k }
    this.nextListenerId = 1;
  }
  
  setEncoded(encoded) {
    return this.setState(this.decode(encoded));
  }
  
  /* Same as patchState, except fields missing from (newState) are defaulted.
   */
  setState(newState) {
    return this.patchState({ ...this.newBlankState(), ...newState });
  }
  
  /* Any missing fields in newState, we preserve existing value.
   * Triggers callbacks.
   * Returns true if anything actually changed.
   */
  patchState(newPartialState) {
    const newState = { ...this.state };
    const changedK = new Set();
    for (const k of Object.keys(newPartialState)) {
      const ov = this.state[k];
      const nv = this.sanitizeValue(k, newPartialState[k]);
      if (ov === nv) continue;
      newState[k] = nv;
      changedK.add(k);
    }
    if (!changedK.size) return false;
    this.state = newState;
    for (const { k, cb } of this.listeners) {
      if (!k || changedK.has(k)) {
        cb();
      }
    }
    return true;
  }
  
  sanitizeValue(k, v) {
    const def = StateService.TOC.find(d => d.k === k);
    if (!def) throw new Error(`State contains unknown key ${JSON.stringify(k)}`);
    switch (def.t) {
      case "int": {
          v = ~~v;
          if (v < 0) return 0;
          const limit = 1 << (def.s * 6);
          if (v >= limit) return limit - 1;
        } break;
      case "str": {
          // It's tempting to truncate to the limit, but limit is for the encoded value. Don't bother.
          // They get quietly truncated at encode if necessary.
          v = v?.toString() || "";
        } break;
    }
    return v;
  }
  
  /* Empty string as (k) to listen for all changes.
   * When your callback fires, the change and any others riding along with it is already in (this.state).
   * Exceptions in (cb) may cause other listeners to miss changes -- try not to do that. Pun intended.
   */
  listen(k, cb) {
    const id = this.nextListenerId++;
    this.listeners.push({ k, cb, id });
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p >= 0) {
      this.listeners.splice(p, 1);
    }
  }
  
  newBlankState() {
    const state = {};
    for (const { k, t } of StateService.TOC) {
      switch (t) {
        case "str": state[k] = ""; break;
        case "int": state[k] = 0; break;
        default: throw new Error(`Unexpected type ${JSON.stringify(t)} for key ${JSON.stringify(k)} in StateService.TOC`);
      }
    }
    return state;
  }
  
  /* Encoded format:
   * Starts with a bitmap of fields present. 6 bits per digit, big-endian.
   * After that, present fields are packed in that order.
   * (s) tells you how many encoded digits for the value (int) or the length (str).
   * For (str), length is in encoded digits.
   */
  
  decode(encoded) {
    const state = {};
    if (!encoded || (encoded.length <= StateService.BITMAP_LENGTH)) return state;
    let bmp = 0, payp = StateService.BITMAP_LENGTH;
    let bmv = this.decodeDigit(encoded[bmp]);
    let bmmask = 0x20;
    for (const def of StateService.TOC) {
      if (bmv & bmmask) {
        payp = this.decodeField(state, def, encoded, payp);
      }
      if (bmmask === 0x01) {
        bmmask = 0x20;
        bmp++;
        bmv = this.decodeDigit(encoded[bmp]);
      } else {
        bmmask >>= 1;
      }
    }
    return state;
  }
  
  // On success, adds field to (state) and returns new (p).
  decodeField(state, def, encoded, p) {
    if (p + def.s > encoded.length) {
      throw new Error(`Invalid encoded state, reading ${JSON.stringify(def)} at ${p}/${encoded.length}`);
    }
    const v = this.decodeMulti(encoded, p, def.s);
    p += def.s;
    switch (def.t) {
      case "int": state[def.k] = v; break;
      case "str": state[def.k] = this.decodeString(encoded, p, v); p += v; break;
      default: throw new Error(`Invalid type '${def.t}'`);
    }
    return p;
  }
  
  decodeString(src, p, c) {
    const dst = new Uint8Array(Math.ceil(c * 3 / 4));
    let dstc = 0;
    while (c >= 4) {
      const src0 = this.decodeDigit(src[p++]);
      const src1 = this.decodeDigit(src[p++]);
      const src2 = this.decodeDigit(src[p++]);
      const src3 = this.decodeDigit(src[p++]);
      dst[dstc++] = (src0 << 2) | (src1 >> 4);
      dst[dstc++] = (src1 << 4) | (src2 >> 2);
      dst[dstc++] = (src2 << 6) | src3;
      c -= 4;
    }
    switch (c) {
      case 1: {
          const src0 = this.decodeDigit(src[p++]);
          dst[dstc++] = (src0 << 2);
        } break;
      case 2: {
          const src0 = this.decodeDigit(src[p++]);
          const src1 = this.decodeDigit(src[p++]);
          dst[dstc++] = (src0 << 2) | (src1 >> 4);
          dst[dstc++] = (src1 << 4);
        } break;
      case 3: {
          const src0 = this.decodeDigit(src[p++]);
          const src1 = this.decodeDigit(src[p++]);
          const src2 = this.decodeDigit(src[p++]);
          dst[dstc++] = (src0 << 2) | (src1 >> 4);
          dst[dstc++] = (src1 << 4) | (src2 >> 2);
          dst[dstc++] = (src2 << 6);
        } break;
    }
    // NUL doesn't trim (why the fuck not?), so pad with spaces if we didn't fill it.
    // Also, the final byte can be NUL already.
    if (dstc && !dst[dstc-1]) dst[dstc-1] = 0x20;
    while (dstc < dst.length) dst[dstc++] = 0x20;
    return this.textDecoder.decode(dst).trim();
  }
  
  decodeMulti(src, p, c) {
    let v = 0;
    for (; c-->0; p++) {
      const sub = this.decodeDigit(src[p]);
      v <<= 6;
      v |= sub;
    }
    return v;
  }
  
  decodeDigit(src) {
    const v = StateService.ALPHABET.indexOf(src);
    if (v < 0) throw new Error(`Invalid character ${JSON.stringify(src)} in encoded state.`);
    return v;
  }
  
  encode(state) {
    if (!state) state = this.state;
    let bm = "", pay = "";
    let bmmask = 0x20, bmtmp = 0;
    for (const def of StateService.TOC) {
      if (state[def.k]) {
        bmtmp |= bmmask;
        pay += this.encodeField(def, state[def.k]);
      }
      if (bmmask === 0x01) {
        bm += StateService.ALPHABET[bmtmp];
        bmtmp = 0;
        bmmask = 0x20;
      } else {
        bmmask >>= 1;
      }
    }
    if (bmmask !== 0x20) bm += StateService.ALPHABET[bmtmp];
    return bm + pay;
  }
  
  encodeField(def, v) {
    switch (def.t) {
      case "int": return this.encodeInt(v, def.s);
      case "str": {
          let text = this.encodeStr(v);
          const limit = 1 << (def.s * 6);
          if (text.length >= limit) text = text.substring(0, limit);
          text = text.trim();
          return this.encodeInt(text.length, def.s) + text;
        }
      default: throw new Error(`Invalid type ${def.t}`);
    }
  }
  
  encodeStr(src) {
    let dst = "";
    const raw = this.textEncoder.encode(src);
    let shift = 0, buffer = 0, rawp = 0;
    while (rawp < raw.length) {
      shift -= 6;
      if (shift < 0) {
        buffer <<= 8;
        shift += 8;
        buffer |= raw[rawp];
        rawp++;
      }
      dst += StateService.ALPHABET[(buffer >> shift) & 0x3f];
    }
    if (shift) {
      shift += 2;
      buffer <<= 8;
      dst += StateService.ALPHABET[(buffer >> shift) & 0x3f];
    }
    return dst;
  }
  
  encodeInt(v, c) {
    let dst = "";
    let shift = 6 * c;
    while (shift > 0) {
      shift -= 6;
      dst += StateService.ALPHABET[(v >> shift) & 0x3f];
    }
    return dst;
  }
}

StateService.singleton = true;

StateService.ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-";

/* Schedule of the state's content.
 * {
 *   k: Key in state object.
 *   t: Type. "str" | "int"
 *   s: Size, in base64 digits. For strings, it's the size of the length field, ie the maximum length.
 * }
 */
StateService.TOC = [
  
  { k: "page",          t: "int", s: 1 },
  { k: "detailsGameid", t: "int", s: 5 },
  
  { k: "searchText",        t: "str", s: 1 },
  { k: "searchList",        t: "str", s: 1 },
  { k: "searchPlatform",    t: "str", s: 1 },
  { k: "searchAuthor",      t: "str", s: 1 },
  { k: "searchGenre",       t: "str", s: 1 },
  { k: "searchFlagsTrue",   t: "int", s: 5 },
  { k: "searchFlagsFalse",  t: "int", s: 5 },
  { k: "searchRating",      t: "str", s: 1 },
  { k: "searchPubtime",     t: "str", s: 1 },
  { k: "searchSort",        t: "str", s: 1 },
  { k: "searchPage",        t: "int", s: 2 },
];

StateService.BITMAP_LENGTH = Math.ceil(StateService.TOC.length / 6);
