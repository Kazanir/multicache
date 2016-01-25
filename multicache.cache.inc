<?php

class MultiCache extends DrupalDatabaseCache implements DrupalCacheInterface {
  protected $bin;

  /**
   * Constructs a MultiCache object.
   *
   * @param $bin
   *   The cache bin for which the object is created.
   *
   * @param $local
   *   The local cache class being used if possible.
   */
  function __construct($bin, $local = NULL) {
    // @todo: Construct an appropriate local cache object to use.
    if (class_exists($local)) {
      $this->local = new $local($bin);
    }
    else {
      throw new Exception("Class $local not available for construction of local cache bin.");
    }

    parent::__construct($bin);
  }

  /**
   * Implements DrupalCacheInterface::get().
   */
  function get($cid) {
    // Do garbage collection.
    $this->garbageCollection($this->bin);

    // Check the local cache for this cid.
    $item = $this->local->get($cid);
    $hash = $this->hash($item->data);

    // Query the item.
    $result = db_query("SELECT cid, IF(hash = :hash, data, NULL) AS data, created, expire, serialized, hash FROM {" . db_escape_table($this->bin) . "} WHERE cid = :cid;", array(":cid" => $cid, ":hash" => $hash))->fetch();

    // If the item doesn't exist in the DB, it has been cleared.
    if (!$result) {
      return FALSE;
    }
    // If the item from the DB has a different hash, prepare and return it
    // instead. The local version is out of date.
    elseif ($hash != $result->hash) {
      $item = $this->prepareItem($result);
      $this->local->set($cid, $item->data, $item->expire);
    }

    return $item;
  }

  /**
   * Implements DrupalCacheInterface::set().
   */
  function set($cid, $data, $expire = CACHE_PERMANENT) {
    $fields = array(
      'serialized' => 0,
      'created' => REQUEST_TIME,
      'expire' => $expire,
      'hash' => $this->hash($data),
    );
    if (!is_string($data)) {
      $fields['data'] = serialize($data);
      $fields['serialized'] = 1;
    }
    else {
      $fields['data'] = $data;
      $fields['serialized'] = 0;
    }

    try {
      db_merge($this->bin)
        ->key(array('cid' => $cid))
        ->fields($fields)
        ->execute();
      // If that succeeds, set the local version as well.
      $this->local->set($cid, $data, $expire);
    }
    catch (Exception $e) {
      // The database may not be available, so we'll ignore cache_set requests.
    }
  }



}

