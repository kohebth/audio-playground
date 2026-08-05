# Audio Playground web tools

The web editor is a React/Vite client for the Audio Playground v2 project and unit contracts.

## Local development

Install dependencies and start the ordinary localhost server:

```sh
npm install
npm run dev
```

Browsers treat `http://localhost` and `http://127.0.0.1` as secure contexts. A phone opening the same server through a
LAN address such as `http://192.168.1.20:5173` is not in a secure context, so the browser does not expose microphone
capture there. Use the trusted HTTPS setup below whenever another device needs the microphone.

## Trusted HTTPS on a LAN

Install [mkcert](https://github.com/FiloSottile/mkcert), find the development computer's LAN IP, and create a certificate
that includes that exact IP:

```sh
mkdir -p .cert
mkcert -install
mkcert -cert-file .cert/lan-cert.pem -key-file .cert/lan-key.pem localhost 127.0.0.1 ::1 192.168.1.20
```

Replace `192.168.1.20` with the address the phone will open. Create `.env.lan-https.local` inside `web-tools/`:

```dotenv
APG_HTTPS_CERT=.cert/lan-cert.pem
APG_HTTPS_KEY=.cert/lan-key.pem
```

The `.cert/` directory and `*.local` environment files are ignored by Git. No private key or local certificate belongs
in the repository.

Run the HTTPS server:

```sh
npm run dev:https
```

Then open `https://192.168.1.20:5173` on the phone. The phone must trust mkcert's development root CA; merely bypassing a
certificate warning does not create a browser secure context. `mkcert -CAROOT` prints the directory containing
`rootCA.pem`. Transfer only that root certificate to the phone, install it as a trusted user CA, and follow the device
vendor's certificate-trust steps. Never transfer `rootCA-key.pem`.

After loading the page, `window.isSecureContext` should be `true` in the browser console and the Mic transport will be
enabled. If the LAN IP changes, issue a new certificate containing the new address.

## Verification

```sh
npm test
npm run typecheck
npm run lint
npm run build
```

The production GitHub Pages deployment already uses HTTPS and is unaffected by the opt-in LAN development mode.
