use mist_qos::*;

#[test]
fn test_fec_recovery_single_loss() {
    let mut encoder = FecEncoder::new(4);
    let mut decoder = FecDecoder::new();

    let p1 = b"packet 1 data".to_vec();
    let p2 = b"packet 2 data longer".to_vec();
    let p3 = b"packet 3".to_vec();
    let p4 = b"packet 4 last".to_vec();

    encoder.add_media_packet(10, &p1);
    encoder.add_media_packet(11, &p2);
    encoder.add_media_packet(12, &p3);
    let fec = encoder
        .add_media_packet(13, &p4)
        .expect("FEC packet expected");

    // Simulate losing p2 (seq 11)
    decoder.add_media_packet(10, &p1);
    // packet 11 is lost
    decoder.add_media_packet(12, &p3);
    decoder.add_media_packet(13, &p4);
    decoder.add_fec_packet(fec);

    let recovered = decoder.try_recover_packets();
    assert_eq!(recovered.len(), 1);
    assert_eq!(recovered[0].0, 11);
    assert_eq!(recovered[0].1, p2);
    assert_eq!(decoder.packets_recovered(), 1);
}

#[test]
fn test_paced_sender_priorities() {
    let mut pacer = PacedSender::new(LowLatencyProfile::default());

    // High priority audio/control packet
    pacer.enqueue_packet(b"audio".to_vec(), 100, true);
    // Normal video packet
    pacer.enqueue_packet(b"video".to_vec(), 100, false);

    // High priority must come out first
    let pkt1 = pacer
        .get_next_packet_to_transmit(100)
        .expect("expected packet");
    assert_eq!(pkt1, b"audio");

    let pkt2 = pacer
        .get_next_packet_to_transmit(100)
        .expect("expected packet");
    assert_eq!(pkt2, b"video");
}

#[test]
fn test_adaptive_bitrate_congestion_backoff() {
    let mut abr = AdaptiveBitrateController::new(QualityProfile::Adaptive);
    let initial_bitrate = abr.current_bitrate_kbps();

    // Normal conditions
    abr.estimator_mut().update_rtt(15.0);
    abr.estimator_mut().update_loss(0.0);
    assert_eq!(abr.estimator().quality_score(), 5);

    // Heavy packet loss occurs over multiple reports
    for _ in 0..15 {
        abr.estimator_mut().update_loss(12.0);
        abr.estimator_mut().update_rtt(200.0);
    }
    assert_eq!(abr.estimator().quality_score(), 1);
    assert_eq!(abr.congestion_state(), CongestionState::CriticallyCongested);

    // Tick should drop bitrate
    let new_bitrate = abr.tick(300).expect("bitrate should reconfigure");
    assert!(new_bitrate < initial_bitrate);
}
