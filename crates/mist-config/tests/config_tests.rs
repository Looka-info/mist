use mist_config::{AppConfig, VideoConfig, MAX_BITRATE_KBPS, MIN_BITRATE_KBPS};

#[test]
fn test_video_config_bitrate_clamping() {
    let mut cfg = VideoConfig {
        bitrate_kbps: 500, // Below minimum
        fps: 0,
        ..Default::default()
    };
    cfg.validate();
    assert_eq!(cfg.bitrate_kbps, MIN_BITRATE_KBPS);
    assert_eq!(cfg.fps, 60);

    let mut cfg_high = VideoConfig {
        bitrate_kbps: 50_000, // Above maximum
        fps: 120,
        ..Default::default()
    };
    cfg_high.validate();
    assert_eq!(cfg_high.bitrate_kbps, MAX_BITRATE_KBPS);
    assert_eq!(cfg_high.fps, 120);
}

#[test]
fn test_app_config_roundtrip() {
    let mut app_cfg = AppConfig::default();
    app_cfg.video.bitrate_kbps = 18_000;
    app_cfg.video.fps = 120;
    app_cfg.ui.dark_theme = false;

    let serialized = serde_json::to_string_pretty(&app_cfg).expect("Serialization failed");
    let deserialized: AppConfig =
        serde_json::from_str(&serialized).expect("Deserialization failed");

    assert_eq!(deserialized.video.bitrate_kbps, 18_000);
    assert_eq!(deserialized.video.fps, 120);
    assert!(!deserialized.ui.dark_theme);
}
