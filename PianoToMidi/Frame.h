#pragma once

class Frame
{
public:
	explicit Frame(AVFrame* frame) : frame_(frame), isFrameReceived_(false) {}
	~Frame()
	{
		assert("Frame::Receive() was not called."
			&& "Then what for did you instantiate Frame object?" && isFrameReceived_);
		if (isFrameReceived_) av_frame_unref(frame_);
	}
	int Receive(AVCodecContext* context)
	{
		assert("Frame::Receive() must be called just once,"
			&& "then Destructor will unreference it" && !isFrameReceived_);
		isFrameReceived_ = true;
		return avcodec_receive_frame(context, frame_);
	}
private:
	AVFrame* frame_;
	bool isFrameReceived_;
	const BYTE padding_[sizeof(INT_PTR) - sizeof(bool)]{ 0 };

	Frame operator=(const Frame&) = delete;
};