#ifndef FLV_H
#define FLV_H

#include <QIODevice>
#include <QVector>

namespace Flv {

using std::unique_ptr;
using std::shared_ptr;

// read from big endian
double readDouble(QIODevice&);
uint32_t readUInt32(QIODevice&);
uint32_t readUInt24(QIODevice&);
uint16_t readUInt16(QIODevice&);
uint8_t readUInt8(QIODevice&);

// write as big endian
void writeDouble(QIODevice&, double);
void writeUInt32(QIODevice&, uint32_t);
void writeUInt24(QIODevice&, uint32_t);
void writeUInt16(QIODevice&, uint16_t);
void writeUInt8(QIODevice&, uint8_t);

namespace TagType { enum {
    Audio = 8,
    Video = 9,
    Script = 18
}; }

namespace SoundFormat { enum {
    AAC = 10
}; }

namespace AacPacketType { enum {
    SequenceHeader = 0,
    Raw = 1
}; }

namespace VideoFrameType { enum {
    Keyframe = 1,
    InterFrame = 2,
    DisposableInterFrame = 3,
    GeneratedKeyframe = 4,
    VideoInfoOrCmdFrame = 5
}; }

namespace VideoCodecId { enum {
    AVC = 7,
    HEVC = 12
}; }

namespace AvcPacketType { enum {
    SequenceHeader = 0,
    NALU = 1,
    EndOfSequence = 2
}; }

namespace AmfValueType { enum {
    Number        = 0, // DOUBLE (8 bytes)
    Boolean       = 1, // UInt8
    String        = 2,
    Object        = 3,
    MovieClip     = 4,
    Null          = 5,
    Undefined     = 6,
    Reference     = 7, // UI16
    EcmaArray     = 8,
    ObjectEndMark = 9,
    StrictArray   = 10,
    Date          = 11,
    LongString    = 12
}; }


class FileHeader
{
public:
    static constexpr auto BytesCnt = 9;

    bool valid;
    uint8_t version;
    union {
        struct {
            uint8_t video     : 1;
            uint8_t reserved1 : 1;
            uint8_t audio     : 1;
            uint8_t reserved5 : 5;
        };
        uint8_t typeFlags;
    };
    uint32_t dataOffset;

    FileHeader(QIODevice &in);
    void writeTo(QIODevice &out);
};


class TagHeader
{
public:
    static constexpr int BytesCnt = 11;

    union {
        struct {
            uint8_t tagType  : 5;
            uint8_t filter   : 1;
            uint8_t reserved : 2;
        };
        uint8_t flags;
    };
    uint32_t dataSize; // UInt24
    int timestamp;

    TagHeader() {}
    bool readFrom(QIODevice &in);
    void writeTo(QIODevice &out);

    TagHeader(QIODevice &in) { readFrom(in); }
    TagHeader(int tagType_, uint32_t dataSize_, int timestamp_)
        : flags(static_cast<uint8_t>(tagType_)), dataSize(dataSize_), timestamp(timestamp_) {}
};


class AudioTagHeader
{
public:
    QByteArray rawData;
    bool isAacSequenceHeader;

    AudioTagHeader(QIODevice &in);
    void writeTo(QIODevice &out);
};


class VideoTagHeader
{
public:
    QByteArray rawData;
    uint8_t codecId;
    uint8_t frameType;
    uint8_t avcPacketType;
    bool isKeyFrame();
    bool isAvcSequenceHeader();

    VideoTagHeader(QIODevice &in);
    void writeTo(QIODevice &out);
};

void writeAvcEndOfSeqTag(QIODevice &out, int timestamp);



class AmfValue
{
public:
    const int type;
    AmfValue(int type_) : type(type_) {}

    ~AmfValue() = default;

    virtual void writeTo(QIODevice& out)
    {
        writeUInt8(out, static_cast<uint8_t>(type));
    };
};


unique_ptr<AmfValue> readAmfValue(QIODevice &in);


class ScriptBody
{
public:
    unique_ptr<AmfValue> name;
    unique_ptr<AmfValue> value;

    ScriptBody(QIODevice &in);
    bool isOnMetaData() const;

    void writeTo(QIODevice &out);
};


class AmfNumber : public AmfValue
{
public:
    double val;
    AmfNumber(double val_) : AmfValue(AmfValueType::Number), val(val_) {}
    AmfNumber(QIODevice &in) : AmfValue(AmfValueType::Number) { val = readDouble(in); }
    void writeTo(QIODevice &out) override { AmfValue::writeTo(out); writeDouble(out, val); }
};


class AmfBoolean : public AmfValue
{
public:
    bool val;
    AmfBoolean(bool val_) : AmfValue(AmfValueType::Boolean), val(val_) {}
    AmfBoolean(QIODevice &in) : AmfValue(AmfValueType::Boolean) { val = readUInt8(in); }
    void writeTo(QIODevice &out) override { AmfValue::writeTo(out); writeUInt8(out, val); }
};


class AmfString : public AmfValue
{
public:
    QByteArray data;
    AmfString(QByteArray data_) : AmfValue(AmfValueType::String), data(std::move(data_)) {}
    AmfString(QIODevice &in);
    void writeTo(QIODevice &out) override;
    static void writeStrWithoutValType(QIODevice &out, const QByteArray &data);
};


class AmfLongString : public AmfValue
{
public:
    QByteArray data;
    AmfLongString(QIODevice &in);
    void writeTo(QIODevice &out) override;
};


class AmfReference : public AmfValue
{
public:
    uint16_t val;
    AmfReference(QIODevice &in) : AmfValue(AmfValueType::Reference) { val = readUInt16(in); }
    void writeTo(QIODevice &out) override { AmfValue::writeTo(out); writeUInt16(out, val); }
};


class AmfDate : public AmfValue
{
public:
    // double dateTime;
    // int16_t localDateTimeOffset;
    QByteArray rawData;
    AmfDate(QIODevice &in) : AmfValue(AmfValueType::Date) { rawData = in.read(10); }
    void writeTo(QIODevice &out) override { AmfValue::writeTo(out); out.write(rawData); }
};


class AmfObjectProperty
{
public:
    QByteArray name;
    unique_ptr<AmfValue> value;
    AmfObjectProperty() {}
    AmfObjectProperty(QIODevice &in);
    void writeTo(QIODevice &out);
    static void write(QIODevice &out, const QByteArray &name, AmfValue *value);

    bool isObjectEnd();
    static void writeObjectEndTo(QIODevice &out);
};


class AmfEcmaArray : public AmfValue
{
    std::vector<AmfObjectProperty> properties;

public:
    AmfEcmaArray() : AmfValue(AmfValueType::EcmaArray) {}
    AmfEcmaArray(std::vector<AmfObjectProperty> &&properties);

    AmfEcmaArray(QIODevice &in);
    void writeTo(QIODevice &out) override;

    unique_ptr<AmfValue>& operator[] (QByteArray name);
};


class AmfObject : public AmfValue
{
    std::vector<AmfObjectProperty> properties;

public:
    AmfObject() : AmfValue(AmfValueType::Object) {}
    AmfObject(QIODevice &in);
    void writeTo(QIODevice &out) override;

    /**
     * @brief moves properties to a new AmfEcmaArray. notice: anchors are not moved
     */
    unique_ptr<AmfEcmaArray> moveToEcmaArray();

    /**
     * @brief Applied on random access output device.
     * - WriteTo(QIODevice &out) writes an empty array followed by a spacer array.
     *   Pointer to `out` and position in file are stored.
     * - Later, appendNumber() would append a number to the first array and reduce
     *   the size of spacer array.
     */
    class ReservedArrayAnchor
    {
        QByteArray name;
        int currentSize = 0;
        QIODevice *outDev = nullptr;
        qint64 arrBeginPos;
        qint64 arrEndPos;

        QByteArray spacerName() const;

    public:
        const int maxSize;

        ReservedArrayAnchor(QByteArray name_, int maxSize_)
            :name(std::move(name_)), maxSize(maxSize_) {}

        int size() { return currentSize; }
        void writeTo(QIODevice &out);
        void appendNumber(double val);
    };

    shared_ptr<ReservedArrayAnchor> insertReservedNumberArray(QByteArray name, int maxSize);

private:
    std::vector<shared_ptr<ReservedArrayAnchor>> anchors;
};


class AmfStrictArray : public AmfValue
{
public:
    /* QVector (an alias for QList since Qt 6) do not accept move-only objects.
     * This is "Unfixable because QVector does implicit sharing." (copy on write?)
     *                          --from https://bugreports.qt.io/browse/QTBUG-57629
     * Therefore std::vector is used instead.
     */
    std::vector<unique_ptr<AmfValue>> values;
    AmfStrictArray() : AmfValue(AmfValueType::StrictArray) {}
    AmfStrictArray(QIODevice &in);
    void writeTo(QIODevice &out) override;
};


class AnchoredAmfNumber : public AmfNumber
{
public:
    class Anchor {
        friend class AnchoredAmfNumber;
        qint64 pos;
        QIODevice *outDev = nullptr;
    public:
        Anchor() {}
        void update(double val);
    };

    AnchoredAmfNumber(double val = 0);
    void writeTo(QIODevice &out) override;
    shared_ptr<Anchor> getAnchor();

private:
    shared_ptr<Anchor> anchor;
};

} // namespace Flv




class QFileDevice;

/**
 * @brief The FlvLiveDownloadDelegate class performs remuxing on the input FLV stream:
 * -# Makes timestamps start from 0. (Timestamps in raw data of Bilibili Live is the time since live started)
 *    Timestamp starting from non-zero causes:
 *    - extremely slow seeking for PotPlayer
 *    - video unable to seek for VLC
 * -# Adds keyframes array at the beginning of file. This occupies about 100 KB,
 *    which is enough for 5 hours if the interval of keyframes is 3 seconds.
 *    If keyframes array is full, data is written to another file.
 *
 * hevc is not supported.
 */
class FlvLiveDownloadDelegate
{
    QIODevice &in;
    std::unique_ptr<QFileDevice> out;

public:
    static constexpr auto MaxKeyframes = 6000;
    static constexpr auto LeastKeyframeInterval = 2500; // ms
    using CreateFileHandler = std::function<std::unique_ptr<QFileDevice>()>;


    FlvLiveDownloadDelegate(QIODevice &in_, CreateFileHandler createFileHandler_);
    ~FlvLiveDownloadDelegate();

    /**
     * @brief Inform that new data is available. Data is read and written to file if there is a whole FLV tag. \n
     * Usage: call newDataArrived() in the slot of in.QIODevice::readyRead (connected outside this class),
     *        then handle the error if this function returns false.
     * @return true if no error, otherwise false
     */
    bool newDataArrived();
    void stop();

    QString errorString();
    qint64 getDurationInMSec();
    qint64 getReadBytesCnt();

private:
    enum class Error { NoError, FlvParseError, SaveFileOpenError, HevcNotSupported };
    Error error = Error::NoError;

    enum class State
    {
        Begin, ReadingTagHeader, ReadingTagBody, ReadingDummy, Stopped
    };

    State state = State::Begin;
    qint64 bytesRequired;
    qint64 readBytesCnt = 0;
    CreateFileHandler createFileHandler;

    bool openNewFileToWrite();
    void closeFile();
    /**
     * @brief call this function when a new keyframe video tag is about to be written
     */
    void updateMetaDataKeyframes(qint64 filePos, int timeInMSec);
    void updateMetaDataDuration();

    bool handleFileHeader();
    bool handleTagHeader();
    bool handleTagBody();
    bool handleScriptTagBody();
    bool handleAudioTagBody();
    bool handleVideoTagBody();


    bool isTimestampBaseValid = false;
    int timestampBase;
    int curFileAudioDuration = 0; // ms
    int curFileVideoDuration = 0; // ms
    qint64 totalDuration = 0;     // ms
    int prevKeyframeTimestamp = -LeastKeyframeInterval;

    Flv::TagHeader tagHeader;

    using FlvArrayAnchor = Flv::AmfObject::ReservedArrayAnchor;
    using FlvNumberAnchor = Flv::AnchoredAmfNumber::Anchor;

    std::shared_ptr<FlvArrayAnchor> keyframesFileposAnchor;
    std::shared_ptr<FlvArrayAnchor> keyframesTimesAnchor;
    std::shared_ptr<FlvNumberAnchor> durationAnchor;

    std::unique_ptr<Flv::ScriptBody> onMetaDataScript;
    QByteArray fileHeaderBuffer;
    QByteArray aacSeqHeaderBuffer;
    QByteArray avcSeqHeaderBuffer;
};

#endif // FLV_H
