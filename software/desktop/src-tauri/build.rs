
fn main() {
    //tonic_build::compile_protos("ground.proto").unwrap();
    tonic_build::configure()
        .build_server(false)
        .compile(
            &["ground.proto", "values.proto"],
            &["../../proto/"],
        ).unwrap();
    tauri_build::build()
}
