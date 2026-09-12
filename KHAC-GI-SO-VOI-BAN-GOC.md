# Bản này khác gì so với Lotus gốc

Đây là bản fork của [fcitx5-lotus](https://github.com/LotusInputMethod/fcitx5-lotus), dùng hằng
ngày trên máy CachyOS + KDE Plasma Wayland. Tệp này ghi lại **từng miếng vá**: vá gì, vì sao, đã
gửi ngược lên chưa, và tác giả trả lời ra sao.

## Lấy bản nào

**Nhánh `ban-dung`.** Đó là nhánh duy nhất nên lấy để dùng hoặc để thử trên máy khác.

```
git clone https://github.com/nguyenphivn/fcitx5-lotus.git
cd fcitx5-lotus
git checkout ban-dung
```

`ban-dung` = `upstream/dev` + đúng **18 miếng vá**, không thiếu commit nào của tác giả.

Mấy nhánh khác là nhánh làm việc, **đừng lấy**:

| Nhánh | Là gì | Có nên lấy |
| --- | --- | --- |
| `ban-dung` | Bản gom gọn, đang dùng hằng ngày | **Có** |
| `thu/bo-fixack` | Nhánh cũ, mã y hệt `ban-dung` nhưng lịch sử có 8 commit làm-rồi-rút-lại | Không |
| `tong-hop`, `tong-hop-v2` | Các lần gom trước, đã cũ | Không |
| `pr/*`, `fix/*`, `do/*`, `thu/*` | Từng nhánh nhỏ để gửi PR hoặc để đo | Không |

Dựng và chạy bộ kiểm:

```
cmake -B build-test -DCMAKE_BUILD_TYPE=Release
cmake --build build-test -j8
unshare -Urn ctest --test-dir build-test        # phải ra 11/11
```

Bản gốc `dev` chạy 9 bài. Bản này 11, vì có thêm hai bài kiểm ở nhóm D bên dưới.

## Nhóm A — lỗi gặp thật, đã báo, tác giả từ chối vá

Hai miếng này tác giả đã đóng issue mà không sửa mã, và cả hai vẫn đứng vững. Mục thứ ba bên
dưới là một miếng vá **đã bị rút lại** vì mình báo sai — giữ lại để khỏi ai làm lại.

### ĐÃ RÚT LẠI — `f9ccb50` máy chủ đừng tự bật chạm-để-bấm (issue #494)

**Miếng vá này đã bị gỡ khỏi `ban-dung`.** Giữ mục này lại làm bài học, đừng làm lại.

Mình báo issue #494 với tiền đề "máy mình tắt chạm-để-bấm", rồi viết thêm một bình luận phản biện
cũng dựa trên tiền đề đó. **Tiền đề đó SAI.** Chủ máy chưa bao giờ tắt chạm-để-bấm; mình tự suy
ra mà không hỏi. Tác giả upstream đúng ngay từ phản hồi đầu tiên.

Đo lại bằng hai dụng cụ, mỗi cái nhìn một ngữ cảnh:

| Nơi | chạm-để-bấm | tắt-khi-đang-gõ |
| --- | --- | --- |
| KWin, tức desktop thật (`busctl` hỏi `org.kde.KWin`) | BẬT, và mặc định cũng BẬT | BẬT |
| Ngữ cảnh libinput riêng dựng y như máy chủ, không ép gì | TẮT | BẬT |

Nên hai dòng ép bật tap trong máy chủ đang làm ngữ cảnh riêng **KHỚP** với desktop, chứ không
phải ghi đè lên nó. Gỡ chúng đi mới là làm Lotus lệch khỏi phần còn lại của máy: người dùng chạm
để dời con trỏ, hệ thống bấm thật, mà Lotus không reset từ đang gõ dở.

**Sai ở đâu, cụ thể:** số đo "16 cú bấm xuống còn 2" là đúng, nhưng mình rút ra kết luận sai từ
nó. Nó chỉ chứng minh **mặc định của THIẾT BỊ** là tắt tap. Nó không nói gì về **cài đặt hiệu lực
của người dùng**, thứ nằm trong ngữ cảnh riêng của compositor và không có cách nào suy ra từ ngữ
cảnh khác. Mình còn suy thêm một tầng nữa cũng sai: thấy `kcminputrc` không có khoá `TapToClick`
rồi kết luận là tắt, trong khi không có khoá nghĩa là **theo mặc định của KDE**, mà mặc định đó
là BẬT.

**Bài học, áp cho mọi lần sau:** cài đặt hiệu lực của người dùng thì **hỏi chủ máy hoặc hỏi
compositor**, đừng suy từ tệp cấu hình và đừng suy từ mặc định của thiết bị. Câu lệnh hỏi thẳng:

```
busctl --user get-property org.kde.KWin \
  /org/kde/KWin/InputDevice/eventN org.kde.KWin.InputDevice tapToClick
```

**Triệu chứng gốc vẫn chưa có lời giải chắc chắn.** Chủ máy báo gõ `Nguyễn Trãi` trong Lark ra
`Nguyễn Traix`, chập chờn. Giả thuyết còn lại mạnh nhất: Lark chạy trong Firefox, mà lúc đó luật
đặt `firefox=3` tức Super Smooth — chế độ uinput duy nhất bị tắt lá chắn chống nhân đôi chữ. Đó
đúng là thứ sau này gây lặp chữ ở thanh địa chỉ Firefox và đã sửa bằng `firefox=1`. Nếu vậy thì
touchpad vô can từ đầu. **Chưa kiểm chứng**, cần chủ máy gõ lại trong Lark rồi báo.

### `770f02e` — bỏ chờ retry vô ích ở app không có surrounding text

**Nguyên nhân:** đường uinput chờ thêm 6 ms để app gửi lại surrounding text, ngay cả với app đã
tự khai là không có. Cái chờ đó không thể thành công.

**Đo:** trên Edge chạy qua XWayland, độ trễ trung vị 12,8 → 6,9 ms, 16/16 lượt đi vào đúng
nhánh được vá.

**Upstream:** [issue #490](https://github.com/LotusInputMethod/fcitx5-lotus/issues/490) — đóng
kiểu NOT_PLANNED.

**Ghi chú quan trọng:** lần đo ĐẦU kết luận sai là "không đổi gì", vì bộ đích chỉ có app Wayland
thuần. App Wayland luôn khai có surrounding text nên không bao giờ đi vào nhánh này. Phải đo
trên app qua XWayland mới thấy. Đừng lặp lại lỗi đó.

### `9c82072` — nhật ký xả đĩa mỗi dòng, không chỉ khi có cảnh báo

**Nguyên nhân:** bản gốc chỉ xả đĩa khi mức log từ WARN trở lên. Mọi dòng quan trọng lúc khởi
động đều là mức INFO, nên nhật ký đứng yên trong khi máy chủ vẫn chạy bình thường. Người đọc
không phân biệt được nhật ký cũ với máy chủ đã chết.

**Giá phải trả:** vài dòng mỗi phút, không đo được.

**Upstream:** [issue #468](https://github.com/LotusInputMethod/fcitx5-lotus/issues/468) — đóng
kiểu NOT_PLANNED.

## Nhóm B — đang chờ tác giả trả lời

Hai commit, gửi chung ở [PR #492](https://github.com/LotusInputMethod/fcitx5-lotus/pull/492)
(bản nháp). Liên quan hai issue còn mở, #487 và #488.

- **`9252326`** — chờ SỰ KIỆN surrounding text thay vì ngủ theo một hằng số đoán trước.
  **Mặc định TẮT**, phải bật trong cấu hình mới có tác dụng.
- **`edcc370`** — ở nhánh dự phòng của chế độ Smooth, chờ app bằng hẹn giờ thay vì `sleep_for`.
  `sleep_for` chặn vòng lặp sự kiện của cả fcitx5, không riêng Lotus.

**Khe hở đã khai thẳng trong PR, đừng rút lại:** nếu người dùng đổi cửa sổ trong vòng 50 ms thì
ô cũ có thể mất chữ. Cửa sổ quá hẹp nên thực tế không gặp.

## Nhóm C — dọn dẹp và hạ tầng, chưa gửi upstream

- **`48e3122` bỏ phụ thuộc X11 không dùng.** Không có dòng mã nào trong `src/`, `server/`,
  `test/` gọi X11. Đo `ldd`: 0 thư viện X11 cả trước lẫn sau, tệp sinh ra giống hệt. Cái đổi là
  máy Wayland thuần không còn phải cài gói phát triển X11 để dựng thứ chẳng đụng tới X11.
  Đối chứng dương cho phép đo: `ldd` trên mô-đun xcb của fcitx5 ra 8 thư viện.
- **`a87bc7b` siết cứng dịch vụ systemd.** `systemd-analyze security` từ 7.0 MEDIUM xuống 2.0
  OK, và đã cài chạy thật để chắc dịch vụ không hỏng. Hai chỉ thị cố ý KHÔNG bật vì cả hai đều
  làm hỏng dịch vụ: `PrivateNetwork` (udev gửi sự kiện qua netlink, netlink theo từng không gian
  mạng) và `ProtectProc` (cổng xác thực phải đọc `/proc/<pid>/exe` của tiến trình thuộc người
  dùng khác). Lưu ý `PrivateTmp` làm nhật ký chuyển vào `/tmp/systemd-private-*/`, đọc phải có
  quyền root và đường cũ ngừng cập nhật.
- **`0355cfb` gỡ `FixUinputWithAck`, cờ Chromium và tệp `src/ack-apps.h`.** Công tắc này vốn mặc
  định TẮT nên gỡ đi hành vi không đổi. Đã soi cả 5 chỗ dùng để chắc mỗi chỗ đặc biệt hoá đúng
  cho nhánh tắt.
- **`86ea551` lấy đường dẫn máy chủ từ CMake thay vì viết cứng `/usr/bin`.**
  ⚠️ **Gửi lẻ commit này lên upstream sẽ làm gói Nix GÃY**, vì Nix dùng
  `substituteInPlace --replace-fail` trên đúng chuỗi literal đó. Phải gửi kèm bản sửa tệp Nix
  trong cùng một PR.
- **`220b57c` biến môi trường `LOTUS_SERVER_PATH`** để chỉ định máy chủ mong đợi. Thiếu biến này
  thì mô-đun từ chối socket chuột, và tính năng bấm chuột ngắt từ chết âm thầm.
- **`6925601` máy chủ hiểu `LOTUS_SOCKET_NAMESPACE`** giống mô-đun. Nhờ vậy chạy được một cặp
  mô-đun + máy chủ riêng bên cạnh bản đóng gói sẵn, không giẫm chân nhau.
- **`f14c0da` núm vặn `LOTUS_BACKSPACE_GAP_MS`** để đo nhịp gửi phím xoá.
- **`d2a46e6` khoảng cách phím xoá mặc định 0 ms thay vì 5.** Đo `khoang_cach_xoa.py macdinh`:
  0,10 ms, 8/8 trên bốn đích. **Đây là lựa chọn riêng của máy này, không phải đề xuất cho
  upstream** — mức đề xuất cho upstream là 2 ms, vì mức 0 bỏ hẳn yêu cầu khe im lặng. Dấu hiệu
  DUY NHẤT để quay lại mức 2 là **sót chữ hoặc thừa chữ khi gõ nhanh**, không phải cảm giác
  nhanh chậm: chênh 0 với 2 chỉ khoảng 6 ms mỗi lần xoá 4 chữ, dưới ngưỡng cảm nhận.

## Nhóm D — bộ kiểm thêm vào

Sáu commit. Hai bài kiểm mới là lý do bản này chạy 11 bài thay vì 9.

- **`c612050`** bài kiểm bất biến trên chuỗi phím ngẫu nhiên.
- **`b789d26`** đối chứng dương cho các bất biến P2 đến P5, tức chứng minh bài kiểm THẤY được
  lỗi khi lỗi có mặt, chứ không phải xanh vì nó không kiểm gì.
- **`a4c9759`** tái hiện vòng lặp giữ phím của issue #472.
- **`971c5fd`** mô hình đúng phím thô mà cửa sổ nhận được ở chế độ Smooth.
- **`a8b4b09`** tách socket riêng cho `smooth_buffered_key_replay` để hai bài kiểm không giẫm
  chân nhau.
- **`d5a6ab1`** hoà giải các nhánh đã gộp với bộ khung kiểm hiện tại của `dev`.

## Cấu hình nên đặt kèm

Vá mã thôi chưa đủ, hai luật theo app dưới đây mới hết lỗi:

- **`firefox=1`** (Smooth), **không phải 3**. Chế độ 3 là Super Smooth, và đó là chế độ uinput
  **duy nhất** bị tắt lá chắn chống nhân đôi chữ ở ô có tự-điền. Để 3 thì thanh địa chỉ Firefox
  lặp chữ đúng kiểu issue #190. Hai chế độ chỉ khác nhau đúng chỗ lá chắn đó. Lark chạy trong
  Firefox nên cũng theo luật này.
- **`Alacritty=1`** vì cùng lý do.

## Những gì bản này KHÔNG sửa

Nói rõ để khỏi mất công thử lại:

- **Chế độ Surrounding Text vẫn lỗi.** Cơ chế surrounding text vốn không đáng tin: Firefox đẩy
  văn bản xung quanh trễ 58 đến 184 ms. Đã thử hướng "tin vào bộ đệm" và bị hồi quy ở ô soạn
  thảo giàu, đã rút lại. Chế độ 4 hỏng trên Firefox cũng thuộc nhóm này.
- **Lỗi `eê` ở thanh địa chỉ Chromium** là lỗi của Chromium, đã báo lên Chromium số 557316480.
  Không vá được từ phía bộ gõ.
- **Máy chủ uinput chết giữa lúc thay chữ làm bàn phím chết theo.** Đã tái hiện được, bộ đo nằm
  ở `probes/vong_go/may_chu_chet_giua_chung.py` trong repo workbench. Chưa vá, đang trong hàng
  đợi gửi issue.

## Quy ước khi gom lại lần sau

Tác giả đẩy mã rất nhanh, khoảng 163 commit mỗi 30 ngày. Khi cần cập nhật:

**Dựng nhánh MỚI từ `upstream/dev` rồi nhặt lại từng vá.** Đừng gộp chồng lên nhánh cũ. Gộp
chồng làm bản mình tụt lại sau upstream mà không ai để ý, đúng như nhánh `tong-hop` cũ: nó hơn
`dev` 27 commit nhưng lại THIẾU 1 commit của tác giả.

Sau khi gom xong, phép kiểm bắt buộc là **so mã băm cây mã** với nhánh trước đó. Giống nhau thì
việc gom đúng, khác một byte cũng là hỏng:

```
git rev-parse ban-dung^{tree}
git rev-parse <nhánh cũ>^{tree}
```

Lần gom này: 27 commit rút còn 19, bỏ 4 cặp làm-rồi-rút-lại, cây mã trùng khít
`ca93438c9247d503736bf4ce002d2d799fd252a9`, bộ kiểm 11/11.

**Không đổi tên bản fork.** Chữ `lotus` nằm 1592 chỗ ở 95 tệp, và 4 tệp tác giả sửa nhiều nhất
chính là 4 tệp việc đổi tên phải cày nát, nên đổi tên là tự chuốc xung đột mỗi lần cập nhật.
Gói Nix dùng `--replace-fail` nên đổi tên là gãy bản dựng chứ không phải cảnh báo. Tính lại khi
số vá bị từ chối vượt 8 đến 10 cái.
