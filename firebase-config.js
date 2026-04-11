const smartWarmFirebaseConfig = {
  apiKey: "AIzaSyDlAlFqcoLSTjGFFobyAaPoPlkAykgc0mY",
  authDomain: "smartwarm-a4d71.firebaseapp.com",
  databaseURL: "https://smartwarm-a4d71-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "smartwarm-a4d71",
  storageBucket: "smartwarm-a4d71.firebasestorage.app",
  messagingSenderId: "812256723480",
  appId: "1:812256723480:web:ba20ee2e0c0357c503396d",
  measurementId: "G-GMXQ867RFV"
};

const smartWarmApp = firebase.apps.length
  ? firebase.app()
  : firebase.initializeApp(smartWarmFirebaseConfig);

// ✅ GUNA AUTH VARIABLE
const auth = firebase.auth();

// ✅ TAMBAH INI (SESSION ONLY)
auth.setPersistence(firebase.auth.Auth.Persistence.SESSION)
  .then(() => {
    console.log("Session persistence ON");
  })
  .catch((error) => {
    console.error("Error setting persistence:", error);
  });

window.smartWarmFirebase = {
  app: smartWarmApp,
  auth: auth,
  database: firebase.database(),
  signInAnonymously() {
    return auth.signInAnonymously();
  }
};