export namespace config {
	
	export class WindowState {
	    width: number;
	    height: number;
	
	    static createFrom(source: any = {}) {
	        return new WindowState(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.width = source["width"];
	        this.height = source["height"];
	    }
	}

}

export namespace models {
	
	export class AppCount {
	    appName: string;
	    count: number;
	
	    static createFrom(source: any = {}) {
	        return new AppCount(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.appName = source["appName"];
	        this.count = source["count"];
	    }
	}
	export class KeyCount {
	    keyCode: number;
	    keyName: string;
	    count: number;
	
	    static createFrom(source: any = {}) {
	        return new KeyCount(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.keyCode = source["keyCode"];
	        this.keyName = source["keyName"];
	        this.count = source["count"];
	    }
	}
	export class TodaySummary {
	    totalKeys: number;
	    topKeys: KeyCount[];
	    appBreakdown: AppCount[];
	
	    static createFrom(source: any = {}) {
	        return new TodaySummary(source);
	    }
	
	    constructor(source: any = {}) {
	        if ('string' === typeof source) source = JSON.parse(source);
	        this.totalKeys = source["totalKeys"];
	        this.topKeys = this.convertValues(source["topKeys"], KeyCount);
	        this.appBreakdown = this.convertValues(source["appBreakdown"], AppCount);
	    }
	
		convertValues(a: any, classs: any, asMap: boolean = false): any {
		    if (!a) {
		        return a;
		    }
		    if (a.slice && a.map) {
		        return (a as any[]).map(elem => this.convertValues(elem, classs));
		    } else if ("object" === typeof a) {
		        if (asMap) {
		            for (const key of Object.keys(a)) {
		                a[key] = new classs(a[key]);
		            }
		            return a;
		        }
		        return new classs(a);
		    }
		    return a;
		}
	}

}

