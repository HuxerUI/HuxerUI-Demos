package com.huxerui.demos.huxermusic;

import android.app.Application;

public final class MainApplication extends Application {
    static {
        System.loadLibrary(BuildConfig.HUXERUI_APP_LIBRARY);
    }
}
