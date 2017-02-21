package com.bignerdranch.android.ffmpegtest;

import android.app.Application;

import com.qiniu.pili.droid.streaming.StreamingEnv;

/**
 * Created by liujingbo on 17/2/13.
 */

public class StreamingApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        StreamingEnv.init(getApplicationContext());
    }
}
