use mist_bitstream::*;
use std::path::Path;

#[test]
fn test_h264_parse_empty_and_truncated() {
    let nals_empty = H264Parser::parse_nal_units(&[]);
    assert!(nals_empty.is_empty());

    let short_data = [0x00, 0x00, 0x01];
    let nals_short = H264Parser::parse_nal_units(&short_data);
    assert!(nals_short.is_empty());

    let min_nal = [0x00, 0x00, 0x01, 0x09];
    let nals_min = H264Parser::parse_nal_units(&min_nal);
    assert_eq!(nals_min.len(), 1);

    let sps_meta = H264Parser::parse_sps(&[]);
    assert!(!sps_meta.is_valid);
}

#[test]
fn test_h264_parse_synthetic_nal_units() {
    let stream = vec![
        0x00, 0x00, 0x00, 0x01, 0x09, 0x10, 0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E, 0xAB,
        0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x3C, 0x80, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00,
    ];

    let nals = H264Parser::parse_nal_units(&stream);
    assert_eq!(nals.len(), 4);

    assert_eq!(nals[0].nal_type, NalType::Aud);
    assert_eq!(nals[0].start_code_size, 4);

    assert_eq!(nals[1].nal_type, NalType::Sps);
    assert_eq!(nals[1].start_code_size, 4);

    assert_eq!(nals[2].nal_type, NalType::Pps);
    assert_eq!(nals[2].start_code_size, 4);

    assert_eq!(nals[3].nal_type, NalType::IdrSlice);
    assert_eq!(nals[3].start_code_size, 3);

    assert!(H264Parser::is_keyframe_access_unit(&stream));
}

#[test]
fn test_h264_extract_access_units() {
    let stream = vec![
        // Frame 1: AUD + IDR
        0x00, 0x00, 0x00, 0x01, 0x09, 0x10, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84,
        // Frame 2: AUD + Non-IDR
        0x00, 0x00, 0x00, 0x01, 0x09, 0x30, 0x00, 0x00, 0x01, 0x41, 0x9A,
    ];

    let aus = H264Parser::extract_access_units(&stream, 60);
    assert_eq!(aus.len(), 2);

    assert!(aus[0].is_keyframe);
    assert_eq!(aus[0].sequence_number, 1);

    assert!(!aus[1].is_keyframe);
    assert_eq!(aus[1].sequence_number, 2);
}

#[test]
fn test_h264_fixture_annexb() {
    let fixture_path = Path::new("../../tests/fixtures/h264_baseline_720p.bin");
    if fixture_path.exists() {
        let reader = AnnexBReader::open_file(fixture_path, 60).expect("failed to open fixture");
        assert!(!reader.is_empty());
        let info = reader.stream_info();
        assert!(info.total_access_units > 0);
        assert_eq!(info.width, 1280);
        assert_eq!(info.height, 720);
    }
}

#[test]
fn test_hevc_synthetic_nal_and_keyframe() {
    // 00 00 00 01 [byte0, byte1]
    // byte0: nal_unit_type = 19 (IdrWRadl) -> (19 << 1) = 38 = 0x26
    let stream = vec![
        0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, // VPS (32 << 1 = 64 = 0x40)
        0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, // SPS (33 << 1 = 66 = 0x42)
        0x00, 0x00, 0x00, 0x01, 0x26, 0x01, 0xAF, // IDR_W_RADL (19 << 1 = 38 = 0x26)
    ];

    let nals = HevcParser::parse_nal_units(&stream);
    assert_eq!(nals.len(), 3);
    assert_eq!(nals[0].nal_type, HevcNalType::Vps);
    assert_eq!(nals[1].nal_type, HevcNalType::Sps);
    assert_eq!(nals[2].nal_type, HevcNalType::IdrWRadl);

    assert!(HevcParser::is_keyframe_access_unit(&stream));
}

#[test]
fn test_av1_leb128_and_obus() {
    let val: u64 = 65535;
    let mut encoded = Vec::new();
    let written = Av1Parser::write_leb128(val, &mut encoded);
    assert!(written > 0);

    let mut offset = 0;
    let decoded = Av1Parser::read_leb128(&encoded, &mut offset).expect("leb128 read failed");
    assert_eq!(decoded, val);

    // Synthetic AV1 sequence header OBU
    // header: obu_forbidden_bit=0, type=1(SequenceHeader), extension=0, size_field=1
    // (1 << 3) | 2 = 0x0A
    let mut obu_stream = vec![0x0A];
    let payload = vec![0x00, 0x00, 0x00, 0x00]; // profile 0
    Av1Parser::write_leb128(payload.len() as u64, &mut obu_stream);
    obu_stream.extend_from_slice(&payload);

    let obus = Av1Parser::parse_obus(&obu_stream);
    assert_eq!(obus.len(), 1);
    assert_eq!(obus[0].obu_type, Av1ObuType::SequenceHeader);
    assert!(Av1Parser::is_keyframe_access_unit(&obu_stream));
}
