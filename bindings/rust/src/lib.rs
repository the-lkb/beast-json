#[cxx::bridge(namespace = "qbuem::rust")]
mod ffi {
    unsafe extern "C++" {
        include!("qbuem_rust_shim.hpp");

        type PyDocument;
        type Value;
        type ObjectIterator;

        fn create_doc() -> UniquePtr<PyDocument>;
        fn parse(doc: Pin<&mut PyDocument>, json: &str) -> Result<()>;
        fn root(doc: &PyDocument) -> UniquePtr<Value>;

        // Standalone functions for Value access (shim handles the dispatch)
        fn is_valid(v: &Value) -> bool;
        fn is_null(v: &Value) -> bool;
        fn is_bool(v: &Value) -> bool;
        fn is_int(v: &Value) -> bool;
        fn is_double(v: &Value) -> bool;
        fn is_string(v: &Value) -> bool;
        fn is_array(v: &Value) -> bool;
        fn is_object(v: &Value) -> bool;
        
        fn size(v: &Value) -> usize;
        fn type_name(v: &Value) -> &str;

        fn get_key(v: &Value, key: &str) -> UniquePtr<Value>;
        fn get_idx(v: &Value, idx: usize) -> UniquePtr<Value>;
        fn at_path(v: &Value, path: &str) -> UniquePtr<Value>;

        // Iteration
        fn create_iter(v: &Value) -> UniquePtr<ObjectIterator>;
        fn iter_next(it: Pin<&mut ObjectIterator>) -> bool;
        fn iter_key(it: &ObjectIterator) -> &str;
        fn iter_value(it: &ObjectIterator) -> UniquePtr<Value>;

        fn as_bool(v: &Value) -> bool;
        fn as_i64(v: &Value) -> i64;
        fn as_f64(v: &Value) -> f64;
        fn as_str(v: &Value) -> &str;
        
        fn dump(v: &Value, indent: i32) -> String;

        // Mutation
        fn set_null(v: Pin<&mut Value>);
        fn set_bool(v: Pin<&mut Value>, b: bool);
        fn set_int(v: Pin<&mut Value>, i: i64);
        fn set_double(v: Pin<&mut Value>, d: f64);
        fn set_string(v: Pin<&mut Value>, s: &str);

        fn insert_raw(v: Pin<&mut Value>, key: &str, raw_json: &str);
        fn erase_key(v: Pin<&mut Value>, key: &str);
        fn erase_idx(v: Pin<&mut Value>, idx: usize);
    }
}

pub struct Document(cxx::UniquePtr<ffi::PyDocument>);

impl Document {
    pub fn new() -> Self {
        Document(ffi::create_doc())
    }

    pub fn parse(&mut self, json: &str) -> Result<(), cxx::Exception> {
        ffi::parse(self.0.pin_mut(), json)
    }

    pub fn root(&self) -> Value {
        Value(ffi::root(&self.0))
    }
}

pub struct Value(cxx::UniquePtr<ffi::Value>);

impl Value {
    pub fn is_valid(&self) -> bool { ffi::is_valid(&self.0) }
    pub fn is_null(&self) -> bool { ffi::is_null(&self.0) }
    pub fn is_bool(&self) -> bool { ffi::is_bool(&self.0) }
    pub fn is_int(&self) -> bool { ffi::is_int(&self.0) }
    pub fn is_double(&self) -> bool { ffi::is_double(&self.0) }
    pub fn is_string(&self) -> bool { ffi::is_string(&self.0) }
    pub fn is_array(&self) -> bool { ffi::is_array(&self.0) }
    pub fn is_object(&self) -> bool { ffi::is_object(&self.0) }

    pub fn size(&self) -> usize { ffi::size(&self.0) }
    pub fn type_name(&self) -> &str { ffi::type_name(&self.0) }

    pub fn get(&self, key: &str) -> Value { Value(ffi::get_key(&self.0, key)) }
    pub fn index(&self, idx: usize) -> Value { Value(ffi::get_idx(&self.0, idx)) }
    pub fn at(&self, path: &str) -> Value { Value(ffi::at_path(&self.0, path)) }

    pub fn items(&self) -> ObjectItems {
        ObjectItems {
            iter: ffi::create_iter(&self.0),
        }
    }

    pub fn set_null(&mut self) { ffi::set_null(self.0.pin_mut()) }
    pub fn set_bool(&mut self, b: bool) { ffi::set_bool(self.0.pin_mut(), b) }
    pub fn set_int(&mut self, i: i64) { ffi::set_int(self.0.pin_mut(), i) }
    pub fn set_double(&mut self, d: f64) { ffi::set_double(self.0.pin_mut(), d) }
    pub fn set_string(&mut self, s: &str) { ffi::set_string(self.0.pin_mut(), s) }

    pub fn insert_raw(&mut self, key: &str, raw_json: &str) {
        ffi::insert_raw(self.0.pin_mut(), key, raw_json)
    }
    pub fn erase_key(&mut self, key: &str) {
        ffi::erase_key(self.0.pin_mut(), key)
    }
    pub fn erase_idx(&mut self, idx: usize) {
        ffi::erase_idx(self.0.pin_mut(), idx)
    }

    pub fn as_bool(&self) -> bool { ffi::as_bool(&self.0) }
    pub fn as_i64(&self) -> i64 { ffi::as_i64(&self.0) }
    pub fn as_f64(&self) -> f64 { ffi::as_f64(&self.0) }
    pub fn as_str(&self) -> &str { ffi::as_str(&self.0) }

    pub fn dump(&self, indent: i32) -> String { ffi::dump(&self.0, indent) }
}

pub struct ObjectItems {
    iter: cxx::UniquePtr<ffi::ObjectIterator>,
}

impl Iterator for ObjectItems {
    type Item = (String, Value);

    fn next(&mut self) -> Option<Self::Item> {
        if self.iter.is_null() {
            return None;
        }
        if ffi::iter_next(self.iter.pin_mut()) {
            let key = ffi::iter_key(&self.iter).to_string();
            let val = Value(ffi::iter_value(&self.iter));
            Some((key, val))
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_basic_parsing() {
        let mut doc = Document::new();
        doc.parse(r#"{"name": "Alice", "age": 30, "scores": [1, 2, 3]}"#).unwrap();
        
        let root = doc.root();
        assert!(root.is_valid());
        assert!(root.is_object());

        let name = root.get("name");
        assert!(name.is_string());
        assert_eq!(name.as_str(), "Alice");

        let age = root.get("age");
        assert!(age.is_int());
        assert_eq!(age.as_i64(), 30);

        let scores = root.get("scores");
        assert!(scores.is_array());
        assert_eq!(scores.size(), 3);
        assert_eq!(scores.index(0).as_i64(), 1);
        assert_eq!(scores.index(2).as_i64(), 3);
    }

    #[test]
    fn test_type_checks() {
        let mut doc = Document::new();
        doc.parse(r#"[null, true, 42, 3.14, "hello"]"#).unwrap();
        let root = doc.root();
        
        assert!(root.index(0).is_null());
        assert!(root.index(1).is_bool());
        assert!(root.index(2).is_int());
        assert!(root.index(3).is_double());
        assert!(root.index(4).is_string());
    }

    #[test]
    fn test_mutation_and_at() {
        let mut doc = Document::new();
        doc.parse(r#"{"a": 1, "b": {"c": 2}}"#).unwrap();
        let mut root = doc.root();
        
        assert_eq!(root.at("/b/c").as_i64(), 2);
        
        root.set_string("modified");
        assert_eq!(root.as_str(), "modified");
    }

    #[test]
    fn test_iteration() {
        let mut doc = Document::new();
        doc.parse(r#"{"x": 10, "y": 20}"#).unwrap();
        let root = doc.root();
        
        let mut items: Vec<(String, i64)> = root.items()
            .map(|(k, v)| (k, v.as_i64()))
            .collect();
        items.sort_by(|a, b| a.0.cmp(&b.0));
        
        assert_eq!(items.len(), 2);
        assert_eq!(items[0], ("x".to_string(), 10));
        assert_eq!(items[1], ("y".to_string(), 20));
    }
}
