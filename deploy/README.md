# Deploying the emulator on OpenShift

The emulator is static files plus one WebAssembly module, so there is nothing to
build: the site goes into a ConfigMap, and a Deployment serves it with a small
static server behind a Route.

## Create the ConfigMap

A ConfigMap key cannot contain a slash, so the files are stored under flat keys
and the Deployment's volume projects them back into the paths the page fetches
(`units/index.json`, `units/<unit>.wasm`).

```sh
cd web
make                      # builds www/units/<unit>.wasm and units/index.json
kubectl create configmap drumlogue-web -n <namespace> \
  --from-file=index.html=www/index.html \
  --from-file=app.js=www/app.js \
  --from-file=worklet.js=www/worklet.js \
  --from-file=styles.css=www/styles.css \
  --from-file=units-index.json=www/units/index.json \
  --from-file=unit-wasm=www/units/<unit>.wasm
```

## Deploy

```sh
kubectl apply -f deploy/drumlogue-web.yaml -n <namespace>
kubectl get route drumlogue-web -n <namespace> \
  -o jsonpath='https://{.spec.host}{"\n"}'
```

The Route terminates TLS at the edge, which is required: `AudioWorklet` is only
available in a secure context, so the page has to be served over HTTPS (or from
`localhost`). The certificate comes from the cluster's wildcard for the apps
domain.

The Deployment runs as a non-root user assigned by the cluster's `restricted`
SCC and mounts the site read-only. If the cluster uses a namespace-specific SCC
range, no `runAsUser` is set on purpose - pinning one breaks the SCC check.

## Adding more units

Build them (`make UNIT=<unit>`), add their wasm to the ConfigMap under another
`unit-<name>` key, add a matching `items:` entry to the Deployment, and roll it:

```sh
kubectl rollout restart deployment/drumlogue-web -n <namespace>
```

Several units fit comfortably in one ConfigMap (they are tens of kilobytes
each); past a megabyte in total, put the wasm files in an image or an object
store instead.
