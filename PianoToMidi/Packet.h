#pragma once

class Packet
{
public:
	explicit Packet(AVPacket* packet) : packet_(packet), isPacketRead_(false) {}
	~Packet()
	{
		assert("Packet::Read was not called."
			&& "Then what for did you instantiate Packet object?" && isPacketRead_);
		if (isPacketRead_) av_packet_unref(packet_);
	}
	int Read(AVFormatContext* context)
	{
		assert("Packet::Read must be called just once,"
			&& "then Destructor will unreference it" && !isPacketRead_);
		isPacketRead_ = true;
		return av_read_frame(context, packet_);
	}
private:
	AVPacket* packet_;
	bool isPacketRead_;
	const BYTE padding_[sizeof(INT_PTR) - sizeof(bool)]{ 0 };

	const Packet& operator=(const Packet&) = delete;
};