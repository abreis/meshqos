// TODO: doc
class VideoData : public AppData {
private:
	float dist_;	// distortion importance
//	char ftype_;	// frame type
public:
//	VideoData(float distortion, char type) : AppData(VOD_DATA), dist_(0), ftype_(0)
	VideoData() : AppData(VOD_DATA), dist_(0) {};
	VideoData(float distortion) : AppData(VOD_DATA)
		{ dist_ = distortion; }
	~VideoData() {}
	float distortion() { return dist_; }
	int size() const { return sizeof(VideoData); }
	AppData* copy() {
		AppData *dup = new VideoData(dist_);
		return dup;
	}
	VideoData& operator= (const VideoData &vodinfo) {
		if(&vodinfo != this){
			dist_ = vodinfo.dist_;
//			ftype_ = vodinfo.ftype_;
		}
		return *this;
	}
};

