package com.panda3ds.pandroid.data.game;

import com.panda3ds.pandroid.R;

public enum GameRegion {
    NorthAmerican,
    Japan,
    Europe,
    Australia,
    China,
    Korean,
    Taiwan,
    None;

    public  int localizedName(){
        switch (this){
            case NorthAmerican:
                return R.string.region_north_armerican;
            case Japan:
                return R.string.region_japan;
            case Europe:
                return R.string.region_europe;
            case Australia:
                return R.string.region_australia;
            case Korean:
                return R.string.region_korean;
            case Taiwan:
                return R.string.region_taiwan;
        }
        return R.string.unknown;
    }
}
