package com.bignerdranch.android.ffmpegtest;

import android.app.Activity;
import android.hardware.Camera;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Environment;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import com.qiniu.android.dns.DnsManager;
import com.qiniu.android.dns.IResolver;
import com.qiniu.android.dns.NetworkInfo;
import com.qiniu.android.dns.http.DnspodFree;
import com.qiniu.android.dns.local.AndroidDnsServer;
import com.qiniu.android.dns.local.Resolver;
import com.qiniu.pili.droid.streaming.AVCodecType;
import com.qiniu.pili.droid.streaming.StreamStatusCallback;
import com.qiniu.pili.droid.streaming.StreamingManager;
import com.qiniu.pili.droid.streaming.StreamingProfile;
import com.qiniu.pili.droid.streaming.StreamingSessionListener;
import com.qiniu.pili.droid.streaming.StreamingState;
import com.qiniu.pili.droid.streaming.StreamingStateChangedListener;
import com.qiniu.pili.droid.streaming.av.common.PLFourCC;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.URISyntaxException;
import java.nio.ByteBuffer;
import java.util.List;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MainActivity extends Activity implements
        StreamStatusCallback,
        StreamingSessionListener,
        StreamingStateChangedListener,
        GLSurfaceView.Renderer {

    protected static final int MSG_START_STREAMING = 0;
    protected static final int MSG_STOP_STREAMING = 1;
    private static final String TAG = "liujingbo";

    private Button mStartBtn;
    private Button mStopBtn;
    private EditText mInputText;
    private EditText mStreamingUrlText;
    private TextView mStreamingStatus;
    private String mStreamingUrl;
    private String mStatus;
    private byte[] result;
    private boolean isStreamingStarted = false;
    private long pts = 0;
    private boolean isFirstDecode = true;
//    private boolean isReady = false;
//    private File mFile;

    protected StreamingManager mStreamingManager;
    protected StreamingProfile mStreamingProfile;

    protected Handler mHandler = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_START_STREAMING:
                    new Thread(new Runnable() {
                        @Override
                        public void run() {
                            boolean res = mStreamingManager.startStreaming();
                            Log.i(TAG, "res = " + res);
                            isStreamingStarted = true;
                        }
                    }).start();
                    break;
                case MSG_STOP_STREAMING:
                    boolean res = mStreamingManager.stopStreaming();
                    isStreamingStarted = false;
                    Log.i(TAG, "res = " + res);
                    break;
            }
        }
    };


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
            requestWindowFeature(Window.FEATURE_ACTION_BAR_OVERLAY);
        } else {
            requestWindowFeature(Window.FEATURE_NO_TITLE);
        }

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mStartBtn = (Button) findViewById(R.id.start_button);
        mStopBtn = (Button) findViewById(R.id.stop_streaming);
        mInputText = (EditText) findViewById(R.id.input_file);
        mStreamingUrlText = (EditText) findViewById(R.id.streaming_url);
        mStreamingStatus = (TextView) findViewById(R.id.streaming_status);

        mStreamingUrlText.setText("rtmp://pili-publish.liujingbo.echohu.top/liujingbo/liujingbo_zhibo");
        mStreamingUrl = mStreamingUrlText.getText().toString().trim();

        mStreamingProfile = new StreamingProfile();
        try {
            mStreamingProfile.setPublishUrl(mStreamingUrl);
        } catch (URISyntaxException e) {
            e.printStackTrace();
        }
        mStreamingProfile.setVideoQuality(StreamingProfile.VIDEO_QUALITY_MEDIUM2)
                .setAudioQuality(StreamingProfile.AUDIO_QUALITY_MEDIUM2)
                .setAdaptiveBitrateEnable(true)
                .setDnsManager(getMyDnsManager())
                .setEncoderRCMode(StreamingProfile.EncoderRCModes.QUALITY_PRIORITY)
                .setEncodingSizeLevel(StreamingProfile.VIDEO_ENCODING_HEIGHT_720)
                .setStreamStatusConfig(new StreamingProfile.StreamStatusConfig(3))
                .setEncodingOrientation(StreamingProfile.ENCODING_ORIENTATION.LAND)
                .setPreferredVideoEncodingSize(1280, 720)
                //问下这个参数的含义是什么
                .setSendingBufferProfile(new StreamingProfile.SendingBufferProfile(0.2f, 0.8f, 3.0f, 20 * 1000));

        mStreamingManager = new StreamingManager(this, AVCodecType.SW_VIDEO_CODEC);
        mStreamingManager.prepare(mStreamingProfile);
        mStreamingManager.setStreamingSessionListener(this);
        mStreamingManager.setStreamingStateListener(this);
        mStreamingManager.setStreamStatusCallback(this);

//        mStreamingProfile.setPreferredVideoEncodingSize(960, 544)
//                    .setEncodingOrientation(StreamingProfile.ENCODING_ORIENTATION.PORT);
//        mStreamingManager.setStreamingProfile(mStreamingProfile);
//        mStreamingManager.prepare(mStreamingProfile);

//        mFile = new File("/storage/emulated/legacy/test.yuv");

        mStartBtn.setOnClickListener(new View.OnClickListener() {

            @Override
            public void onClick(View v) {
                String folderurl = Environment.getExternalStorageDirectory().getPath();
                String urlTextInput = mInputText.getText().toString();
                String inputUrl = folderurl + "/" + urlTextInput;
                Log.i("liujingbo", inputUrl);
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        decode("/storage/emulated/0/WeChatSight40.mp4");
                    }
                }).start();
                startStreaming();
//                saveYUVFile(result);
//                for(int i = 0; i < result.length; i++){
//                    Log.i("liujingbo",result[i]);
//                }
            }
        });
        mStopBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                stopStreaming();
            }
        });

    }

    @Override
    protected void onResume() {
        super.onResume();
        mStreamingManager.resume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mHandler.removeCallbacksAndMessages(null);
        mStreamingManager.pause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mStreamingManager.destroy();
    }

    protected void startStreaming() {
        mHandler.removeCallbacksAndMessages(null);
        mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_START_STREAMING), 50);
    }

    protected void stopStreaming() {
        mHandler.removeCallbacksAndMessages(null);
        mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_STOP_STREAMING), 50);
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
//    public native String urlprotocolinfo();
//    public native String avformatinfo();
//    public native String avcodecinfo();
//    public native String avfilterinfo();
//    public native String configurationinfo();
    public native byte[] decode(String inputurl);

    private void onDecoder(byte[] yuvData, int width, int height, int rotation, boolean mirror, double tsInNanoTime) {
        Log.i(TAG," isStreamingStarted = " + isStreamingStarted);
//        if(!isStreamingStarted) {
//
//        }
//        try{
//            FileOutputStream fos = new FileOutputStream(mFile,true);
//            fos.write(yuvData,0,yuvData.length);
//            fos.close();
//        }catch (Exception e){
//            e.printStackTrace();
//        }
//        if(isFirstDecode){
//            mStreamingProfile.setPreferredVideoEncodingSize(width,height);
//            isFirstDecode = false;
//            mStreamingManager.setStreamingProfile(mStreamingProfile);
//            Log.i(TAG,"set encoding size");
//        }
        pts = new Double(tsInNanoTime).longValue();
        Log.i(TAG,"pts = " + pts);
        if(isStreamingStarted) {
            Log.i(TAG, "yuvData.length = " + yuvData.length + " width = " + width + " height = " + height + " tsInNanoTime = " + tsInNanoTime);
            mStreamingManager.inputVideoFrame(yuvData, width, height, rotation, mirror, PLFourCC.FOURCC_I420, pts);
        }
    }

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    public void notifyStreamStatusChanged(StreamingProfile.StreamStatus streamStatus) {

    }

    @Override
    public void onStateChanged(StreamingState streamingState, Object o) {
        switch (streamingState) {
            case PREPARING:
                mStatus = "preparing";
                break;
            case READY:
                mStatus = "ready";
//                isReady = true;
                break;
            case CONNECTING:
                mStatus = "connecting";
                break;
            case STREAMING:
                mStatus = "streaming...";
                break;
            case SHUTDOWN:
                mStatus = "shutdown";
                break;
            case IOERROR:
                mStatus = "ioerror";
                break;
            case SENDING_BUFFER_EMPTY:
                break;
            case SENDING_BUFFER_FULL:
                break;
            case DISCONNECTED:
                mStatus += "DISCONNECTED\n";
                break;
            case INVALID_STREAMING_URL:
                Log.e(TAG, "Invalid streaming url:" + o);
                break;
            case UNAUTHORIZED_STREAMING_URL:
                Log.e(TAG, "Unauthorized streaming url:" + o);
                mStatus += "Unauthorized Url\n";
                break;
        }
        Log.i(TAG, mStatus);
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (null != mStatus) {
                    mStreamingStatus.setText(mStatus);
                }
            }
        });
    }

    @Override
    public boolean onRecordAudioFailedHandled(int i) {
        return false;
    }

    @Override
    public boolean onRestartStreamingHandled(int i) {
        return false;
    }

    @Override
    public Camera.Size onPreviewSizeSelected(List<Camera.Size> list) {
        return null;
    }

    private static DnsManager getMyDnsManager() {
        IResolver r0 = new DnspodFree();
        IResolver r1 = AndroidDnsServer.defaultResolver();
        IResolver r2 = null;
        try {
            r2 = new Resolver(InetAddress.getByName("119.29.29.29"));
        } catch (IOException ex) {
            ex.printStackTrace();
        }
        return new DnsManager(NetworkInfo.normal, new IResolver[]{r0, r1, r2});
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {

    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {

    }

    @Override
    public void onDrawFrame(GL10 gl) {

    }
}
