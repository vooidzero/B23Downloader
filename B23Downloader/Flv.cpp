#include "Flv.h"
#include <QtEndian>
#include <QCoreApplication>
#include <QFileDevice>
#include <QBuffer>

using std::shared_ptr;
using std::make_shared;
using std::unique_ptr;
using std::make_unique;

double Flv::readDouble(QIODevice &in)
{
    char data[8];
    in.read(data, 8);
    return qFromBigEndian<double>(data);
}

uint32_t Flv::readUInt32(QIODevice &in)
{
    char data[4];
    in.read(data, 4);
    return qFromBigEndian<uint32_t>(data);
}

uint32_t Flv::readUInt24(QIODevice &in)
{
    char data[4];
    data[0] = 0;
    in.read(data + 1, 3);
    return qFromBigEndian<uint32_t>(data);
}

uint16_t Flv::readUInt16(QIODevice &in)
{
    char data[2];
    in.read(data, 2);
    return qFromBigEndian<uint16_t>(data);
}

uint8_t Flv::readUInt8(QIODevice &in)
{
    uint8_t data;
    in.read(reinterpret_cast<char*>(&data), 1);
    return data;
}

void Flv::writeDouble(QIODevice &out, double val)
{
    val = qToBigEndian<double>(val);
    out.write(reinterpret_cast<char*>(&val), 8);
}

void Flv::writeUInt32(QIODevice &out, uint32_t val)
{
    val = qToBigEndian<uint32_t>(val);
    out.write(reinterpret_cast<char*>(&val), 4);
}

void Flv::writeUInt24(QIODevice &out, uint32_t val)
{
    val = qToBigEndian<uint32_t>(val);
    out.write(reinterpret_cast<char*>(&val) + 1, 3);
}


void Flv::writeUInt16(QIODevice &out, uint16_t val)
{
    val = qToBigEndian<uint16_t>(val);
    out.write(reinterpret_cast<char*>(&val), 2);
}

void Flv::writeUInt8(QIODevice &out, uint8_t val)
{
    out.write(reinterpret_cast<char*>(&val), 1);
}


Flv::FileHeader::FileHeader(QIODevice &in)
{
    constexpr uint32_t FlvSignatureUInt32 = 'F' | ('L' << 8) | ('V' << 16);
    char signature[4] = {0};
    in.read(signature, 3);
    if (*reinterpret_cast<uint32_t*>(signature) != FlvSignatureUInt32) {
        valid = false;
        return;
    }

    valid = true;
    version = readUInt8(in);
    typeFlags = readUInt8(in);
    dataOffset = readUInt32(in);
}

void Flv::FileHeader::writeTo(QIODevice &out)
{
    out.write("FLV", 3);
    writeUInt8(out, version);
    writeUInt8(out, typeFlags);
    writeUInt32(out, dataOffset);
}

bool Flv::TagHeader::readFrom(QIODevice &in)
{
    flags = readUInt8(in);
    if (filter) {
        return false;
    }

    dataSize = readUInt24(in);
    auto tsLow = readUInt24(in);
    auto tsExt = readUInt8(in);
    timestamp = (tsExt << 24) | tsLow;
    readUInt24(in); // stream id, always 0
    return true;
}

void Flv::TagHeader::writeTo(QIODevice &out)
{
    writeUInt8(out, flags);
    writeUInt24(out, dataSize);
    writeUInt24(out, timestamp & 0x00FFFFFF);
    writeUInt8(out, static_cast<uint8_t>(timestamp >> 24));
    writeUInt24(out, 0);
}

Flv::AudioTagHeader::AudioTagHeader(QIODevice &in)
{
    rawData = in.peek(2);
    auto flags = readUInt8(in);
    if (SoundFormat::AAC == ((flags >> 4) & 0xF)) {
        isAacSequenceHeader = (readUInt8(in) == AacPacketType::SequenceHeader);
    } else {
        isAacSequenceHeader = false;
        rawData.chop(1);
    }
}

void Flv::AudioTagHeader::writeTo(QIODevice &out)
{
    out.write(rawData);
}

Flv::VideoTagHeader::VideoTagHeader(QIODevice &in)
{
    rawData = in.peek(5);
    auto byte = readUInt8(in);
    codecId = byte & 0xF;
    frameType = (byte >> 4) & 0xF;
    if (codecId == VideoCodecId::AVC) {
        avcPacketType = readUInt8(in);
        readUInt24(in); // compositionTime (SI24)
    } else {
        avcPacketType = 0xFF;
        rawData.chop(4);
    }
}

bool Flv::VideoTagHeader::isKeyFrame()
{
    return frameType == VideoFrameType::Keyframe;
}

bool Flv::VideoTagHeader::isAvcSequenceHeader()
{
    return (codecId == VideoCodecId::AVC && avcPacketType == AvcPacketType::SequenceHeader);
}

void Flv::VideoTagHeader::writeTo(QIODevice &out)
{
    out.write(rawData);
}

void Flv::writeAvcEndOfSeqTag(QIODevice &out, int timestamp)
{
    TagHeader(TagType::Video, 5, timestamp).writeTo(out);
    const char TagData[5] = {
        0x17, // frameType=1 (Key frame), codecId=7 (AVC)
        0x02, // avcPacketType=AvcEndOfSequence
        0x00, 0x00, 0x00 // compositionTimeOffset=0
    };
    out.write(TagData, 5);
    writeUInt32(out, 16); // prev tag size = 16
}



Flv::ScriptBody::ScriptBody(QIODevice &in)
{
    name = readAmfValue(in);
    value = readAmfValue(in);
}

bool Flv::ScriptBody::isOnMetaData() const
{
    if (name->type != AmfValueType::String) {
        return false;
    }
    return static_cast<AmfString*>(name.get())->data == "onMetaData";
}

void Flv::ScriptBody::writeTo(QIODevice &out)
{
    name->writeTo(out);
    value->writeTo(out);
}

unique_ptr<Flv::AmfValue> Flv::readAmfValue(QIODevice &in)
{
    auto type = readUInt8(in);
    switch (type) {
    case AmfValueType::Number:        return make_unique<AmfNumber>(in);
    case AmfValueType::Boolean:       return make_unique<AmfBoolean>(in);
    case AmfValueType::String:        return make_unique<AmfString>(in);
    case AmfValueType::Object:        return make_unique<AmfObject>(in);
    case AmfValueType::MovieClip:     return make_unique<AmfValue>(AmfValueType::MovieClip);
    case AmfValueType::Null:          return make_unique<AmfValue>(AmfValueType::Null);
    case AmfValueType::Undefined:     return make_unique<AmfValue>(AmfValueType::Undefined);
    case AmfValueType::Reference:     return make_unique<AmfReference>(in);
    case AmfValueType::EcmaArray:     return make_unique<AmfEcmaArray>(in);
    case AmfValueType::ObjectEndMark: return make_unique<AmfValue>(AmfValueType::ObjectEndMark);
    case AmfValueType::StrictArray:   return make_unique<AmfStrictArray>(in);
    case AmfValueType::Date:          return make_unique<AmfDate>(in);
    case AmfValueType::LongString:    return make_unique<AmfLongString>(in);
    default:                          return make_unique<AmfValue>(AmfValueType::Null);
    }
}

Flv::AmfString::AmfString(QIODevice &in) : AmfValue(AmfValueType::String)
{
    auto len = readUInt16(in);
    data = in.read(len);
}

void Flv::AmfString::writeTo(QIODevice &out)
{
    AmfValue::writeTo(out);
    writeUInt16(out, static_cast<uint16_t>(data.size()));
    out.write(data);
}

void Flv::AmfString::writeStrWithoutValType(QIODevice &out, const QByteArray &data)
{
    writeUInt16(out, static_cast<uint16_t>(data.size()));
    out.write(data);
}

Flv::AmfLongString::AmfLongString(QIODevice &in) : AmfValue(AmfValueType::LongString)
{
    auto len = readUInt32(in);
    data = in.read(len);
}

void Flv::AmfLongString::writeTo(QIODevice &out)
{
    AmfValue::writeTo(out);
    writeUInt32(out, static_cast<uint32_t>(data.size()));
    out.write(data);
}

Flv::AmfObjectProperty::AmfObjectProperty(QIODevice &in)
{
    auto nameLen = readUInt16(in);
    name = in.read(nameLen);
    value = readAmfValue(in);
}

void Flv::AmfObjectProperty::writeTo(QIODevice &out)
{
    AmfString::writeStrWithoutValType(out, name);
    value->writeTo(out);
}

void Flv::AmfObjectProperty::write(QIODevice &out, const QByteArray &name, AmfValue *value)
{
    AmfString::writeStrWithoutValType(out, name);
    value->writeTo(out);
}

bool Flv::AmfObjectProperty::isObjectEnd()
{
    return (name.size() == 0 && value->type == AmfValueType::ObjectEndMark);
}

void Flv::AmfObjectProperty::writeObjectEndTo(QIODevice &out)
{
    static const char objEndMark[3] = {0, 0, AmfValueType::ObjectEndMark};
    out.write(objEndMark, 3);
}

Flv::AmfEcmaArray::AmfEcmaArray(std::vector<AmfObjectProperty> &&properties_)
    : AmfValue(AmfValueType::EcmaArray), properties(std::move(properties_))
{
}

Flv::AmfEcmaArray::AmfEcmaArray(QIODevice &in) : AmfValue(AmfValueType::EcmaArray)
{
    readUInt32(in); // ecmaArrayLength, approximate number of items in ECMA array
    while (!in.atEnd()) {
        auto p = AmfObjectProperty(in);
        if (p.isObjectEnd()) {
            break;
        }
        properties.emplace_back(std::move(p));
    }
}

void Flv::AmfEcmaArray::writeTo(QIODevice &out)
{
    AmfValue::writeTo(out);
    writeUInt32(out, static_cast<uint32_t>(properties.size()));
    for (auto &p : properties) {
        p.writeTo(out);
    }
    AmfObjectProperty::writeObjectEndTo(out);
}

unique_ptr<Flv::AmfValue> &Flv::AmfEcmaArray::operator[](QByteArray name)
{
    auto it = std::find_if(
        properties.begin(),
        properties.end(),
        [name](const AmfObjectProperty &obj) { return obj.name == name; }
    );
    if (it != properties.end()) {
        return it->value;
    }
    properties.emplace_back();
    properties.back().name = std::move(name);
    return properties.back().value;
}

Flv::AmfObject::AmfObject(QIODevice &in) : AmfValue(AmfValueType::Object)
{
    while (!in.atEnd()) {
        auto p = AmfObjectProperty(in);
        if (p.isObjectEnd()) {
            break;
        }
        properties.emplace_back(std::move(p));
    }
}

void Flv::AmfObject::writeTo(QIODevice &out)
{
    AmfValue::writeTo(out);
    for (auto &p : properties) {
        p.writeTo(out);
    }
    for (auto &anchor : anchors) {
        anchor->writeTo(out);
    }
    AmfObjectProperty::writeObjectEndTo(out);
}

unique_ptr<Flv::AmfEcmaArray> Flv::AmfObject::moveToEcmaArray()
{
    return make_unique<AmfEcmaArray>(std::move(properties));
}

shared_ptr<Flv::AmfObject::ReservedArrayAnchor>
Flv::AmfObject::insertReservedNumberArray(QByteArray name, int maxSize)
{
    properties.erase(
        std::remove_if(
            properties.begin(),
            properties.end(),
            [&name](const AmfObjectProperty &obj){ return obj.name == name; }
        ),
        properties.end()
    );
    auto anchor = make_shared<ReservedArrayAnchor>(std::move(name), maxSize);
    anchors.emplace_back(anchor);
    return anchor;
}

Flv::AmfStrictArray::AmfStrictArray(QIODevice &in) : AmfValue(AmfValueType::StrictArray)
{
    auto len = readUInt32(in);
    assert(len < 0XFFFF);
    values.reserve(len);
    for (uint32_t i = 0; i < len; i++) {
        values.emplace_back(readAmfValue(in));
    }
}

void Flv::AmfStrictArray::writeTo(QIODevice &out)
{
    writeUInt32(out, static_cast<uint32_t>(values.size()));
    for (auto &val : values) {
        val->writeTo(out);
    }
}

QByteArray Flv::AmfObject::ReservedArrayAnchor::spacerName() const
{
    return name + "Spacer";
}

void Flv::AmfObject::ReservedArrayAnchor::writeTo(QIODevice &out)
{
    currentSize = 0;
    AmfString::writeStrWithoutValType(out, name);
    writeUInt8(out, AmfValueType::StrictArray);
    arrBeginPos = out.pos();
    writeUInt32(out, 0);
    arrEndPos = out.pos();
    AmfString::writeStrWithoutValType(out, spacerName());
    writeUInt8(out, AmfValueType::StrictArray);
    writeUInt32(out, maxSize);

    char element[9]; memset(element, 0, 9);
    for (int i = 0; i < maxSize; i++) {
        // equal to AmfNumber(0).writeTo(out)
        out.write(element, 9);
    }

    if (!out.isSequential()) {
        outDev = &out;
    }
}

void Flv::AmfObject::ReservedArrayAnchor::appendNumber(double val)
{
    // qDebug() << "appending" << name << val;
    if (outDev == nullptr || currentSize == maxSize) {
        return;
    }

    auto pos = outDev->pos();

    currentSize += 1;
    outDev->seek(arrBeginPos);
    writeUInt32(*outDev, currentSize);
    outDev->seek(arrEndPos);
    AmfNumber(val).writeTo(*outDev);
    arrEndPos = outDev->pos();
    AmfString::writeStrWithoutValType(*outDev, spacerName());
    writeUInt8(*outDev, AmfValueType::StrictArray);
    writeUInt32(*outDev, maxSize - currentSize);

    outDev->seek(pos);
}

Flv::AnchoredAmfNumber::AnchoredAmfNumber(double val)
    : AmfNumber(val), anchor(make_shared<Anchor>())
{
}

void Flv::AnchoredAmfNumber::writeTo(QIODevice &out)
{
    writeUInt8(out, AmfValueType::Number);
    auto pos = out.pos();
    writeDouble(out, val);
    if (!out.isSequential()) {
        anchor->outDev = &out;
        anchor->pos = pos;
    }
}

shared_ptr<Flv::AnchoredAmfNumber::Anchor> Flv::AnchoredAmfNumber::getAnchor()
{
    return anchor;
}

void Flv::AnchoredAmfNumber::Anchor::update(double val)
{
    if (outDev == nullptr) {
        return;
    }
    auto tmp = outDev->pos();
    outDev->seek(pos);
    writeDouble(*outDev, val);
    outDev->seek(tmp);
}



FlvLiveDownloadDelegate::FlvLiveDownloadDelegate(QIODevice &in_, CreateFileHandler createFileHandler_)
    :in(in_), createFileHandler(createFileHandler_)
{
    bytesRequired = Flv::FileHeader::BytesCnt + 4; // FlvFileHeader + prevTagSize (UI32)
}

FlvLiveDownloadDelegate::~FlvLiveDownloadDelegate()
{
    if (out != nullptr) {
        closeFile();
    }
}

QString FlvLiveDownloadDelegate::errorString()
{
    switch (error) {
    case Error::NoError:
        return QString();
    case Error::FlvParseError:
        return QStringLiteral("FLV 解析错误");
    case Error::SaveFileOpenError:
        return QStringLiteral("文件打开失败");
    case Error::HevcNotSupported:
        return QStringLiteral("暂不支持 HEVC");
    default:
        return "undefined error";
    }
}

qint64 FlvLiveDownloadDelegate::getDurationInMSec()
{
    return totalDuration + curFileVideoDuration;
}

qint64 FlvLiveDownloadDelegate::getReadBytesCnt()
{
    return readBytesCnt;
}

bool FlvLiveDownloadDelegate::newDataArrived()
{
    bool noError = true;
    while (noError) {
        if (in.bytesAvailable() < bytesRequired) {
            break;
        }
        auto tmp = bytesRequired;
        switch (state) {
        case State::Begin:
            noError = handleFileHeader();
            break;
        case State::ReadingTagHeader:
            noError = handleTagHeader();
            if (noError) {
                state = State::ReadingTagBody;
                bytesRequired = tagHeader.dataSize + 4; // tagBody + prevTagSize (UI32)
            }
            break;
        case State::ReadingTagBody:
            noError = handleTagBody();
            if (noError) {
                state = State::ReadingTagHeader;
                bytesRequired = Flv::TagHeader::BytesCnt;
            }
            break;
        case State::ReadingDummy:
            state = State::ReadingTagHeader;
            in.skip(bytesRequired);
            bytesRequired = Flv::TagHeader::BytesCnt;
            break;
        case State::Stopped:
            return true;
        }
        readBytesCnt += tmp;
    }
    if (!noError) {
        stop();
    }
    return noError;
}

bool FlvLiveDownloadDelegate::openNewFileToWrite()
{
    if (out != nullptr) {
        closeFile();
    }

    out = createFileHandler();
    if (out == nullptr) {
        error = Error::SaveFileOpenError;
        return false;
    }

    out->write(fileHeaderBuffer);

    auto scriptTagHeader = Flv::TagHeader(Flv::TagType::Script, 0, 0);
    auto scriptTagBeginPos = out->pos();
    scriptTagHeader.writeTo(*out);
    onMetaDataScript->writeTo(*out);
    auto scriptTagEndPos = out->pos();
    auto scriptTagSize = scriptTagEndPos - scriptTagBeginPos;
    scriptTagHeader.dataSize = scriptTagSize - Flv::TagHeader::BytesCnt;
    out->seek(scriptTagBeginPos);
    scriptTagHeader.writeTo(*out);
    out->seek(scriptTagEndPos);
    Flv::writeUInt32(*out, scriptTagSize);

    out->write(aacSeqHeaderBuffer);
    out->write(avcSeqHeaderBuffer);
    return true;
}

void FlvLiveDownloadDelegate::updateMetaDataKeyframes(qint64 filePos, int timeInMSec)
{
    keyframesFileposAnchor->appendNumber(filePos);
    keyframesTimesAnchor->appendNumber(timeInMSec / 1000.0);
}

void FlvLiveDownloadDelegate::updateMetaDataDuration()
{
    durationAnchor->update(std::max(curFileVideoDuration, curFileAudioDuration) / 1000.0);
}

void FlvLiveDownloadDelegate::closeFile()
{
    if (!avcSeqHeaderBuffer.isEmpty()) {
        updateMetaDataKeyframes(out->pos(), curFileVideoDuration);
        Flv::writeAvcEndOfSeqTag(*out, curFileVideoDuration);
    }
    updateMetaDataDuration();
    out.reset();
}

void FlvLiveDownloadDelegate::stop()
{
    if (out != nullptr) {
        closeFile();
    }
    state = State::Stopped;
}

bool FlvLiveDownloadDelegate::handleFileHeader()
{
    Flv::FileHeader flvFileHeader(in);
    Flv::readUInt32(in); // read dummy prev tag size (UInt32)
    if (!flvFileHeader.valid) {
        error = Error::FlvParseError;
        return false;
    }

    if (flvFileHeader.dataOffset != Flv::FileHeader::BytesCnt) {
        state = State::ReadingDummy;
        bytesRequired = flvFileHeader.dataOffset - Flv::FileHeader::BytesCnt;
        flvFileHeader.dataOffset = Flv::FileHeader::BytesCnt;
    } else {
        state = State::ReadingTagHeader;
        bytesRequired = Flv::TagHeader::BytesCnt;
    }

    QBuffer buffer(&fileHeaderBuffer);
    buffer.open(QIODevice::WriteOnly);
    flvFileHeader.writeTo(buffer);
    Flv::writeUInt32(buffer, 0);
    return true;
}

bool FlvLiveDownloadDelegate::handleTagHeader()
{
    if (!tagHeader.readFrom(in)) {
        error = Error::FlvParseError;
        return false;
    }
    if (onMetaDataScript == nullptr && tagHeader.tagType != Flv::TagType::Script) {
        error = Error::FlvParseError;
        return false;
    }
    return true;
}

bool FlvLiveDownloadDelegate::handleTagBody()
{
    switch (tagHeader.tagType) {
    case Flv::TagType::Script:
        return handleScriptTagBody();
    case Flv::TagType::Audio:
        return handleAudioTagBody();
    case Flv::TagType::Video:
        return handleVideoTagBody();
    default:
        error = Error::FlvParseError;
        return false;
    }
}

bool FlvLiveDownloadDelegate::handleScriptTagBody()
{
    onMetaDataScript = make_unique<Flv::ScriptBody>(in);
    Flv::readUInt32(in); // read prevTagSize (UInt32)
    if (!onMetaDataScript->isOnMetaData()) {
        error = Error::FlvParseError;
        return false;
    }
    if (onMetaDataScript->value->type == Flv::AmfValueType::Object) {
        onMetaDataScript->value = static_cast<Flv::AmfObject*>(onMetaDataScript->value.get())->moveToEcmaArray();
    } else if (onMetaDataScript->value->type != Flv::AmfValueType::EcmaArray) {
        error = Error::FlvParseError;
        return false;
    }

    auto &ecmaArr = *static_cast<Flv::AmfEcmaArray*>(onMetaDataScript->value.get());
    auto comment = "created by B23Downloader v" + QCoreApplication::applicationVersion().toUtf8();
    comment += " github.com/vooidzero/B23Downloader";
    ecmaArr["Comment"] = make_unique<Flv::AmfString>(comment);

    auto duration = make_unique<Flv::AnchoredAmfNumber>();
    durationAnchor = duration->getAnchor();
    ecmaArr["duration"] = std::move(duration);

    auto keyframesObj = make_unique<Flv::AmfObject>();
    keyframesFileposAnchor = keyframesObj->insertReservedNumberArray("filepositions", MaxKeyframes);
    keyframesTimesAnchor = keyframesObj->insertReservedNumberArray("times", MaxKeyframes);
    ecmaArr["keyframes"] = std::move(keyframesObj);
    return true;
}

bool FlvLiveDownloadDelegate::handleAudioTagBody()
{
    auto audioHeader = Flv::AudioTagHeader(in);
    auto writeTagTo = [this, &audioHeader](QIODevice &outDev) {
        tagHeader.writeTo(outDev);
        audioHeader.writeTo(outDev);
        auto audioDataSize = tagHeader.dataSize - audioHeader.rawData.size();
        outDev.write(in.read(audioDataSize + 4)); // audio data + prevDataSize
    };

    if (audioHeader.isAacSequenceHeader) {
        tagHeader.timestamp = 0;
        auto prevSeqHeader = std::move(aacSeqHeaderBuffer);

        QBuffer buffer(&aacSeqHeaderBuffer);
        buffer.open(QIODevice::WriteOnly);
        writeTagTo(buffer);

        if (!prevSeqHeader.isEmpty() && prevSeqHeader != aacSeqHeaderBuffer) {
            error = Error::FlvParseError;
            return false;
        }
        if (out != nullptr) {
            out->write(aacSeqHeaderBuffer);
        }
    } else {
        if (!isTimestampBaseValid) {
            isTimestampBaseValid = true;
            timestampBase = tagHeader.timestamp;
        }
        tagHeader.timestamp -= timestampBase;
        if (tagHeader.timestamp < curFileAudioDuration) {
            error = Error::FlvParseError;
            return false;
        }
        curFileAudioDuration = tagHeader.timestamp;
        if (out == nullptr && !openNewFileToWrite()) {
            return false;
        }
        writeTagTo(*out);
    }
    return true;
}

bool FlvLiveDownloadDelegate::handleVideoTagBody()
{
    auto videoHeader = Flv::VideoTagHeader(in);
    if (videoHeader.codecId == Flv::VideoCodecId::HEVC) {
        error = Error::HevcNotSupported;
        return false;
    }

    auto writeTagTo = [this, &videoHeader](QIODevice &outDev) {
        tagHeader.writeTo(outDev);
        videoHeader.writeTo(outDev);
        auto audioDataSize = tagHeader.dataSize - videoHeader.rawData.size();
        outDev.write(in.read(audioDataSize + 4)); // audio data + prevDataSize
    };

    if (videoHeader.isAvcSequenceHeader()) {
        tagHeader.timestamp = 0;
        auto prevSeqHeader = std::move(avcSeqHeaderBuffer);

        QBuffer buffer(&avcSeqHeaderBuffer);
        buffer.open(QIODevice::WriteOnly);
        writeTagTo(buffer);

        if (!prevSeqHeader.isEmpty() && prevSeqHeader != avcSeqHeaderBuffer) {
            error = Error::FlvParseError;
            return false;
        }
        if (out != nullptr) {
            out->write(avcSeqHeaderBuffer);
        }

    } else {
        if (!isTimestampBaseValid) {
            isTimestampBaseValid = true;
            timestampBase = tagHeader.timestamp;
        }
        tagHeader.timestamp -= timestampBase;
        if (tagHeader.timestamp < curFileVideoDuration) {
            error = Error::FlvParseError;
            return false;
        }
        curFileVideoDuration = tagHeader.timestamp;
        if (out == nullptr && !openNewFileToWrite()) {
            return false;
        }
        if (videoHeader.isKeyFrame()
            && tagHeader.timestamp - prevKeyframeTimestamp >= LeastKeyframeInterval) {
            auto shouldSeg = keyframesFileposAnchor->size() == keyframesFileposAnchor->maxSize - 1;
            if (shouldSeg) {
                if (!openNewFileToWrite()) {
                    return false;
                }
                totalDuration += curFileVideoDuration;
                timestampBase = timestampBase + curFileVideoDuration;
                curFileAudioDuration = 0;
                curFileVideoDuration = 0;
                tagHeader.timestamp = 0;
            }
            updateMetaDataKeyframes(out->pos(), tagHeader.timestamp);
            updateMetaDataDuration();
            prevKeyframeTimestamp = tagHeader.timestamp;
        }

        writeTagTo(*out);
    }
    return true;
}
