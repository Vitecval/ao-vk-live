<?php
// Run with PHP CLI. WordPress/mail are isolated doubles; no message is sent.
define('ABSPATH', __DIR__);
$options = array(); $transients = array(); $mail = array(); $mail_ok = true; $actions = array(); $passed = array();
class WP_Error { public $code, $data; function __construct($code, $message, $data) {$this->code=$code; $this->data=$data;} }
class WP_REST_Response { public $data, $status; function __construct($data, $status) {$this->data=$data; $this->status=$status;} function header($a,$b){} }
class Request { public $p, $body, $type='application/json'; function __construct($p){$this->p=$p;$this->body=json_encode($p);} function get_body(){return $this->body;} function get_header($n){return $this->type;} function get_json_params(){return $this->p;} }
function get_option($k,$fallback=false){global $options;return array_key_exists($k,$options)?$options[$k]:$fallback;}
function add_option($k,$v,$x='',$autoload=false){global $options;if(array_key_exists($k,$options))return false;$options[$k]=$v;return true;}
function maybe_serialize($v){return serialize($v);}
class Database {public $options='wp_options';function prepare($q,...$args){return $args;}function query($args){global $options;if(isset($options[$args[0]])&&serialize($options[$args[0]])===$args[1])unset($options[$args[0]]);} }
$wpdb=new Database();
function wp_cache_delete($k,$group){}
function get_transient($k){global $transients;return $transients[$k]??false;}
function set_transient($k,$v,$ttl){global $transients;$transients[$k]=$v;}
function wp_salt($scheme){return 'test-only-secret-salt';}
function wp_generate_uuid4(){return bin2hex(random_bytes(16));}
function is_email($s){return is_string($s)&&filter_var($s,FILTER_VALIDATE_EMAIL);}
function sanitize_email($s){return filter_var($s,FILTER_SANITIZE_EMAIL);}
function sanitize_text_field($s){return trim(strip_tags($s));}
function wp_mail($to,$subject,$body,$headers){global $mail,$mail_ok;$mail[]=array($to,$subject,$body,$headers);return $mail_ok;}
function add_action($name,$callback){global $actions;$actions[$name]=$callback;}
function check($ok,$label){global $passed;if(!$ok)throw new Exception($label);$passed[]=$label;}
function error_status($r,$code){return $r instanceof WP_Error&&$r->data['status']===$code;}
require __DIR__.'/../apps/wordpress/horizon-reports/rubika-horizon-reports.php';
function payload($id=null){$text="GPU timing 22ms\nLatest diagnostic excerpt";return array('schema'=>1,'id'=>$id??bin2hex(random_bytes(16)),'version'=>'0.7.0','title'=>'Low FPS','encoding'=>'gzip-base64','bytes'=>strlen($text),'report'=>base64_encode(gzencode($text)));}
function reset_limits(){global $transients;$transients=array();}
$_SERVER['REMOTE_ADDR']='192.0.2.4';
check(error_status(rkh_receive_report(new Request(payload())),503)&&count($mail)===0,'Reporting is disabled until the site owner enables it');
$options['rkh_report_options']=array('enabled'=>true,'email'=>'dev@asyncstuff.com');
$p=payload();$r=rkh_receive_report(new Request($p));
check($r instanceof WP_REST_Response&&$r->data['accepted']&&$r->data['id']===$p['id']&&count($mail)===1,'A valid reviewed anonymous report is accepted by the mail service');
rkh_receive_report(new Request($p));check(count($mail)===1,'Retrying an accepted report ID does not send duplicate email');
$injected=payload();$injected['title']="Text\r\nBcc: outsider@example.test";$injected['to']='outsider@example.test';rkh_receive_report(new Request($injected));
check($mail[1][0]==='dev@asyncstuff.com'&&strpos($mail[1][1],"\r")===false&&strpos($mail[1][1],"\n")===false&&count($mail[1][3])===1,'Client data cannot choose an email recipient or inject mail headers');
$huge=new Request(payload());$huge->body=str_repeat('x',1048577);check(error_status(rkh_receive_report($huge),413),'Oversized requests are refused before JSON processing');
$wrong=new Request(payload());$wrong->type='application/zip';check(error_status(rkh_receive_report($wrong),415),'Arbitrary uploaded file types are refused');
foreach(array('id','version','report','schema') as $field){$bad=payload();$bad[$field]=array('not a string');check(error_status(rkh_receive_report(new Request($bad)),400),'Malformed '.$field.' cannot reach mail');}
$bad=payload();$bad['report']=base64_encode(gzencode("\xff\xfe"));$bad['bytes']=2;check(error_status(rkh_receive_report(new Request($bad)),400),'Invalid decoded UTF-8 is refused');
$bad=payload();$bad['report']=base64_encode(gzencode(str_repeat('x',4194305)));$bad['bytes']=1;check(error_status(rkh_receive_report(new Request($bad)),400),'A compressed payload cannot exceed the decoded size limit');
$bad=payload();$bad['bytes']=4194305;check(error_status(rkh_receive_report(new Request($bad)),400),'An oversized declared decompressed length is refused');
$bad=payload();$bad['bytes']=1;check(error_status(rkh_receive_report(new Request($bad)),400),'The decoded report must match its declared size');
reset_limits();$mail_ok=false;$retry=payload();check(error_status(rkh_receive_report(new Request($retry)),503),'Failed mail is not acknowledged as accepted');$mail_ok=true;$before=count($mail);$r=rkh_receive_report(new Request($retry));check($r instanceof WP_REST_Response&&count($mail)===$before+1,'A failed mail attempt can be retried with the same report ID');
reset_limits();for($i=0;$i<5;$i++)rkh_receive_report(new Request(payload()));$before=count($mail);check(error_status(rkh_receive_report(new Request(payload())),429)&&count($mail)===$before,'Per-address rate limits stop excess anonymous mail');
$_SERVER['HTTP_X_FORWARDED_FOR']='198.51.100.22';check(error_status(rkh_receive_report(new Request(payload())),429),'An arbitrary forwarded-address header cannot bypass rate limits');
reset_limits();$transients['rkh_report_hour_'.gmdate('YmdH')]=60;check(error_status(rkh_receive_report(new Request(payload())),429),'The site-wide hourly limit stops excess mail');
reset_limits();$options['rkh_report_send_lock']=array('token'=>'another sender','until'=>time()+120);check(error_status(rkh_receive_report(new Request(payload())),429),'Concurrent submissions respect the exclusive mail lease');
$old=array('token'=>'expired','until'=>time()-1);$options['rkh_report_send_lock']=$old;$r=rkh_receive_report(new Request(payload()));check($r instanceof WP_REST_Response,'A crashed sender lease can expire and recover');
$options['rkh_report_send_lock']=array('token'=>'new owner','until'=>time()+120);rkh_report_unlock($old);check(get_option('rkh_report_send_lock')['token']==='new owner','An expired sender cannot remove a newer lease');
foreach($options as $value)check(strpos(serialize($value),'Latest diagnostic excerpt')===false,'Report contents are not stored in WordPress options');
echo json_encode(array('result'=>'passed','checks'=>$passed,'emailSent'=>false,'scope'=>'PHP plugin with isolated WordPress/mail doubles; live WordPress delivery is not tested'),JSON_PRETTY_PRINT),"\n";
