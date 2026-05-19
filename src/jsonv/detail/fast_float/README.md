# fast_float (vendored)

Single-header copy of [fast_float](https://github.com/fastfloat/fast_float),
used by `ast_node::decimal::value()` to convert JSON number tokens to
`double` via the Eisel–Lemire algorithm.

## Version

`v8.2.5` (release artifact `fast_float.h`).

## License

fast_float is offered under Apache-2.0 / MIT / BSL-1.0. We take it under
Apache-2.0 to match JSON Voorhees. The upstream `LICENSE-APACHE` is
included alongside `fast_float.h` in this directory.

## Refreshing

```
cd src/jsonv/detail/fast_float
curl -sL -o fast_float.h https://github.com/fastfloat/fast_float/releases/download/<TAG>/fast_float.h
curl -sL -o LICENSE-APACHE https://raw.githubusercontent.com/fastfloat/fast_float/<TAG>/LICENSE-APACHE
```

Then bump the version above and rebuild.
