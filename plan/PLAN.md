# Context

Sistem ini dibuat untuk memenuhi tugas pembuatan expert system berbasis RDR (binomial RDR) yang dapat diterapkan untuk berbagai domain. Dosen secara gamblang menyatakan harus menggunakan agentic AI dalam membuatnya, dan kamu akan melakukannya untuk saya.  

# System Requirements

Kebutuhan sistem ini dibuat oleh saya. Beberapa hal saya tetapkan secara mandatory, dan beberapa saya buat fleksibel sesuai pilihanmu. 

## Kode Program

### Bahasa dan Kompilasi

1. Program merupakan aplikasi GUI yang wajib dapat dijalankan pada sistem berbasis UNIX (termasuk WSL2) dan Windows. 
2. Ditulis menggunakan bahasa C, .c dan .h (mandatory, bukan juga C++) sebagai modul dan main program. Bahasa untuk antarmuka GUI dibebaskan, selama memungkinkan main program tetap ditulis dengan bahasa C. 
3. Program dapat dikompilasi untuk UNIX dan Windows, termasuk WSL2. 
4. Terapkan best practice dalam menulis kode dengan C. Gunakan header dan dokumentasi modul dan tipe data dengan baik. 
5. implementasikan kode secara modular dan terabstraksi sehingga dapat diterapkan pada bidang yang luas. 
6. Dilarang menggunakan library yang sudah mengimplementasikan RDR. Selain itu, tida ada batasan, selama library tersebut tidak berat dan mudah diinstalasi. 
7. Buatlah skrip untuk mengkompilasi di UNIX dan Windows. 
8. Minimasi dependensi yang terlalu banyak, buatlah agar program dapat dijalankan dengan sekali klik file executable.

### Dokumentasi dan Konversi

1. Dokumentasi fungsi, prosedur, dan tipe data dituliskan dalam header dengan lengkap dalam bahasa Indonesia. Istilah dan nama fungsi/prosedur/adt/variabel menggunakan bahasa inggris. 
2. Komen pada baris kode terdiri dari dua jenis. 
- komen terhadap blok proses: ditulis berupa nama proses tersebut (ditulis dalam frasa noun, seperti menjuduli/menamai proses), bukan menjelaskan proses tersebut melakukan apa. conto: `/* Data Load *.` bukan `/* Loading the data to do bla bla */`. komen ini ditulis dengan bahasa inggris. 
- komen yang memperjelas maksud suatu baris kode. ini komen yang ditulis di samping kode. ditulis singkat dalam bahasa indonesia. sifatnya seperti anotasi singkat. hanya gunakan jika baris kode tersebut benar-benar perlu dijelaskan, gunakan seminimal mungkin. 
3. Hindari pengunakan emoji, em dash, semicolon dalam teks dokumentasi/komen dan tampilan program (kecuali untuk ASCII atau UNICODE art). Hindari penggunaan kata `ini` di depan kalimat (gunakan `Hal ini` atau `Inilah yang`). 
4. Dokumentasikan hasil akhir kerjamu dalam folder `docs/`.  

### Testing

Testing terdiri dari 3 jenis test
1. Unit test untuk setiap modul -> kamu buat dan jalankan untuk pengujian. 
2. End-to-end test -> kamu rancang, simpan rancangan testing mu dalam file md. kamu jalankan, debug, dan fix sesuai temuan. 
3. Quality Control -> pengecekan hasil kejamu oleh saya. saya akan mengecek kode dan menjalankan secara manual. tulislah jika ada pengecekan end-to-end yang tidak bisa kamu lakukan tanpa saya. 

## Spesifikasi Fungsional

### Sistem RDR

sistem ini dirancang sesuai dengan pemahaman saya terkait RDR. jika kamu menemukan representasi knowledge RDR yang sudah matang dan dapat diterapkan dengan C, atau kamu ingin mengoreksi pemahaman saya terkait RDR, maka desain ini perlu disesuaikan ulang, dengan diskusi terlebih dahulu. 

#### Implementasi RDR
1. Implementasikan RDR dari scratch, dengan ketentuan kode program yang sudah saya jelaskan. 
2. RDR bekerja dengan adt tree yang node nya merupakan sebuah pernyataan (1 klausa atau lebih) yang memiliki nilai kebenaran true/false. 
3. knowldge RDR harus dapat disimpan dan dimuat dari sebuah file yang human-readable. (pilih satu: json, xml, txt, atau jika ada format file khusus untuk rdr). 
4. dilarang keras menggunakan kode buatan orang lain. 

#### Representasi Tree
1. Node terdiri dari 2 informasi: `<klausa> -> <kesimpulan>`. kesimpulan merupakan sebuah nilai string literal yang tidak terformat, sifatnya hanya ditampilkan dan tidak digunakan dalam reasoning.  
2. Sebuah node memiliki >= 2 edge: true edge dan false edge. (jika dirpint dalam UNICODE tree text, maka true edge adalah sibling yang ada di atas false edge)
3. selain representasi tree, RDR juga harus menyimpan: 
- node terakhir yang dieksekusi (meskipun kesimpulan bukan diambil darinya)
- kueri dari node terakhir yang diambil kesimpulan
4. akuisisi atau update rule dilakukan dengan menambahkan edge (exception) dari node terakhir yang dieksekusi. klausa yang digunakan adalah klausa kueri update yang tidak terpenuhi oleh klausa kueri dari node trakhir yang diambil kesimpulan (yang salah). 

#### Aturan Penulisan Klausa
1. Representasi klausa adalah sebagai berikut:
- simple boolean value -> ditulis berubah sebuah variabel tanpa spasi. ada fakta menyatakan true, tidak ada menyatakan unknown (OWA). contoh: `demam`. operasi `!` digunakan untuk menyatakan not. contoh: `!demam`
- literal value -> ditulis berupa variabel yang di-assign dengan `=` dilanjutkan dengan sebuah nilai literal. setiap variabel, assign, dan literal dipisah dengan spasi. bernilai true jika fakta memiiki nilai literal yang sama (string match) dengan klausa. contoh: `demam = tinggi`. operasi `!=` untuk menunjukkan ketidaksamaan. 
- numeric value -> ditulis berupa sebuah variabel dihubungkan dengan kesamaan dan ketidaksamaan numerik (`=`, `>`, `<`, `>=`, `<=`, `!=`) terhadap sebuah nilai numerik (bisa integer maupun desimal). bernilai ture jika fakta merupakan subset dari klausa. contoh: `derajat_demam >= 38.5`.
2. klausa yang tidak sejenis, tidak dapat dibandingkan, bahkan jika variabel berupa string yang sama. 
3. node dapat terdiri lebih daru satu klausa, yang seluruhnya bersifat `AND`. 

## Pembuatan knowledge baru
1. untuk membuat knowldge baru, user terlebih dahulu harus menetapkan nilai default root `T -> <default>`. Ini adalah satu-satunya yang perlu disesuaikan untuk domain berbeda. contoh untuk penyakit: `T -> Sehat`, contoh untuk penentuan pajak: `T -> 0%`. 
2. selanjutnya knowledge dapat digunakan sambil terus dikembangkan. 
3. selain membuat knowldge baru, user juga bisa melakukan load ke project baru dan save ke file sesuai spesifikasi. 

## Kueri dan Akuisi
1. sintaks kueri merupakan klausa-klausa fakta yang ditulis dengan pemisahan semicolon atau enter. spasi setelah dan sebelum enter dihapus sehingga klausa kueri bersih dari tambahan spasi di awal dan akhirnya. contoh kueri: `orang_kaya; demam <= 38.6 ; budget = besar` akan diambil klausa bersihnya: [`orang_kaya`, `demam <= 38.6`, `budget = besar`]. 
2. setiap klausa yang dikueri terlebih dahulu ditampilkan pada panel, dalam bentuk daftar yang menyediakan opsi untuk menghapus dan mengedit setiap klausa. ada juga pilihan tampilan untuk mengedit kueri dalam bentuk free text. user mengklik `start` untuk memulai reasoning dan mendapatkan hasil. 
3. hasil ditampilkan pada panel lain, menampilkan seluruh kesimpulan yang dia hasilkan, dan kesimpulan terakhir (pembangkitan kesimpulan diurut dari yang paling bawah ke paling atas, kesimpulan paling atas adalh kesimpulan akhir, diberi kejelasan visual). opsi panel juga bisa diubah ke bentuk tree dengan warna font penanda rule yang dibangkitkan (sebelum reasoning, jika fitur ini diklik, hasilnya adalah tree yang tanpa perbedaan warna font)
4. jika user tidak setuju dengan hasil kesimpulan yang didapatkan, user dapat melakukan edit dengan memasukkan kesimpulan yang seharusnya. RDR bekerja dengan membuat exception sesuai ketentuan. 
5. akuisisi/pembentukan knowledge juga dapat dibuat dari dataset dengan format CSV. isinya adalah kolom string rule klausa dipisah dengan smicolon dan spasi, serta kolom kesimpulan. 
6. selain mengubah selayaknya RDR bekerja, sistem juga menyediakan untuk mengubah manual dari tree yang disajikan. 

## Help

menampilkan halaman dokumentasi cara penggunaan program. 