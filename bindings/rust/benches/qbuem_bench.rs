use criterion::{black_box, criterion_group, criterion_main, Criterion};
use qbuem_json::Document;
use serde_json::Value as SerdeValue;

const BENCH_DATA: &str = r#"{
    "users": [
        {"id": 0, "name": "User 0", "active": true, "scores": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]},
        {"id": 1, "name": "User 1", "active": false, "scores": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]},
        {"id": 2, "name": "User 2", "active": true, "scores": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]},
        {"id": 3, "name": "User 3", "active": false, "scores": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]},
        {"id": 4, "name": "User 4", "active": true, "scores": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]}
    ],
    "meta": {"total": 5, "version": "1.0.7", "desc": "Benchmarking data"}
}"#;

fn bench_parsing(c: &mut Criterion) {
    let mut group = c.benchmark_group("Parsing");

    group.bench_function("serde_json", |b| {
        b.iter(|| {
            let _: SerdeValue = serde_json::from_str(black_box(BENCH_DATA)).unwrap();
        })
    });

    group.bench_function("qbuem_json", |b| {
        let mut doc = Document::new();
        b.iter(|| {
            doc.parse(black_box(BENCH_DATA)).unwrap();
        })
    });

    group.finish();
}

fn bench_access(c: &mut Criterion) {
    let mut group = c.benchmark_group("Access");

    // Serde
    let v: SerdeValue = serde_json::from_str(BENCH_DATA).unwrap();
    group.bench_function("serde_json", |b| {
        b.iter(|| {
            let _ = black_box(&v)["users"][2]["name"].as_str().unwrap();
        })
    });

    // Qbuem
    let mut doc = Document::new();
    doc.parse(BENCH_DATA).unwrap();
    let root = doc.root();
    group.bench_function("qbuem_json", |b| {
        b.iter(|| {
            let _ = black_box(root.at("/users/2/name")).as_str();
        })
    });

    group.finish();
}

criterion_group!(benches, bench_parsing, bench_access);
criterion_main!(benches);
