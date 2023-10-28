/* SearchModel.js
 * Dumb object for holding the contents of the search page.
 */
 
import { DbService } from "./DbService.js";
 
export class SearchModel {
  constructor(src) {
    if (!src) src = {};
  
    // Query parameters (exact names), which also live in global state (different names).
    this.text = src.text || "";
    this.list = src.list || "";
    this.platform = src.platform || "";
    this.author = src.author || "";
    this.genre = src.genre || "";
    this.rating = src.rating || "";
    this.pubtime = src.pubtime || "";
    this.flags = src.flags || "";
    this.notflags = src.notflags || "";
    this.page = src.page || 1;
  }
  
  static fromState(state) {
    return new SearchModel({
      text: state.searchText,
      list: state.searchList,
      platform: state.searchPlatform,
      author: state.searchAuthor,
      genre: state.searchGenre,
      rating: state.searchRating,
      pubtime: state.searchPubtime,
      flags: SearchModel.flagNamesFromBits(state.searchFlagsTrue),
      notflags: SearchModel.flagNamesFromBits(state.searchFlagsFalse),
      page: state.searchPage,
    });
  }
  
  toState() {
    return {
      searchText: this.text,
      searchList: this.list,
      searchPlatform: this.platform,
      searchAuthor: this.author,
      searchGenre: this.genre,
      searchRating: this.rating,
      searchPubtime: this.pubtime,
      searchFlagsTrue: SearchModel.flagBitsFromNames(this.flags),
      searchFlagsFalse: SearchModel.flagBitsFromNames(this.notflags),
      searchPage: this.page,
    };
  }
  
  toQuery(addl) {
    const query = { ...addl, ...this };
    for (const key of Object.keys(query)) {
      if (!query[key]) delete query[key];
    }
    return query;
  }
  
  equivalent(other) {
    return (
      (this.text === other.text) &&
      (this.list === other.list) &&
      (this.platform === other.platform) &&
      (this.author === other.author) &&
      (this.genre === other.genre) &&
      (this.rating === other.rating) &&
      (this.pubtime === other.pubtime) &&
      (this.flags === other.flags) &&
      (this.notflags === other.notflags) &&
      (this.page === other.page)
    );
  }
  
  static flagNamesFromBits(bits) {
    let names = "";
    if (bits) for (let mask=1, p=0; p<DbService.FLAG_NAMES.length; mask<<=1, p++) {
      if (bits & mask) names += DbService.FLAG_NAMES[p] + " ";
    }
    return names.trim();
  }
    
  static flagBitsFromNames(names) {
    if (!names) return 0;
    let bits = 0;
    for (const name of names.toLowerCase().split(/\s+/)) {
      const p = DbService.FLAG_NAMES.indexOf(name);
      if (p < 0) continue;
      bits |= 1 << p;
    }
    return bits;
  }
  
}
