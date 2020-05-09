package com.example.playdemo

import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView

interface OnPlayListener {
    fun message(message: String)
    fun onProgress(progress: Int, duration: Int)
}


class Play(listener: OnPlayListener) : SurfaceHolder.Callback {

    private var _surfaceHolder: SurfaceHolder? = null
    private lateinit var _dataSource: String
    private val _listener = listener;
    private var duration: Int = 0;

    fun setSurfaceView(surface: SurfaceView) {
        _surfaceHolder = surface.holder
        //addCallback如果在中途被去掉，可能导致后续监听无法再获取到
        _surfaceHolder!!.addCallback(this)
    }

    fun setDataSource(source: String) {
        _dataSource = source
    }

    /**
     * native 回调给java 播放进去的
     * @param progress
     */
    fun onProgress(progress: Int) {
        _listener.onProgress(progress, getDuration())
    }

    fun onPlayEnd() {
        Log.e("onPlayEnd", "end");
    }

    /**
     * 准备好 要播放的视频
     */
    fun prepare() {
        native_prepare(_dataSource)
    }

    /**
     * 开始播放
     */
    fun start() {
        native_start()
    }

    /**
     * 停止播放
     */
    fun stop() {
        native_stop()
        duration = 0
    }

    fun pause() {
        native_pause()
    }

    fun release() {
        _surfaceHolder?.removeCallback(this)
        _surfaceHolder = null
        native_release()
    }

    fun onError(errorCode: Int) {
        Log.e("onError", errorCode.toString());
        _listener.message(errorCode.toString())
    }

    fun onPrepared() {
        start()
        _listener.message("准备完毕")
    }

    fun getDuration(): Int {
        if (duration == 0) {
            duration = native_getDuration()
        }
        return duration
    }

    fun seek(progress: Int) {
        native_seek(progress)
    }

    override fun surfaceChanged(surfaceHolder: SurfaceHolder?, p1: Int, p2: Int, p3: Int) {
        surfaceHolder?.apply {
            _surfaceHolder = surfaceHolder
            _surfaceHolder!!.addCallback(this@Play)
            native_setSurface(surfaceHolder.surface)
        }
    }

    override fun surfaceDestroyed(p0: SurfaceHolder?) {
//        release()
    }

    override fun surfaceCreated(surfaceHolder: SurfaceHolder?) {
//        Log.e("setSurfaceView", "=============$surfaceHolder")
//        surfaceHolder?.apply {
//            Log.e("_surfaceHolder", "=============$_surfaceHolder")
//            if (_surfaceHolder == null) {
//                _surfaceHolder = surfaceHolder
//                _surfaceHolder!!.addCallback(this@Play)
//                native_setSurface(surfaceHolder.surface)
//            }
//        }
    }


    external fun native_prepare(dataSource: String)
    external fun native_start()
    external fun native_pause()
    external fun native_stop()
    external fun native_release()
    external fun native_setSurface(surface: Surface)
    private external fun native_getDuration(): Int
    private external fun native_seek(progress: Int)

}