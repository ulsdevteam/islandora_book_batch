<?php

class IslandoraBookBatch extends IslandoraScanBatch {
  protected static function get_object_class() {
    return 'IslandoraBookBookBatchObject';
  }

  /**
   * Parses the file listing into the structure we use.
   *
   * @overrides IslandoraScanBatch::group_files()
   */
  protected function group_files($listing) {
    $files = array();

    foreach ($listing as $relative_uri => $file_info) {
      $dir_name = dirname($relative_uri);
      $components = explode(DIRECTORY_SEPARATOR, $dir_name);
      $this->re_group($files, $components, $relative_uri, $file_info);
    }

    // We are not interested in files at the top level:
    unset($files['.']);

    return $files;
  }

  /**
   * Recursively regroup/parse the listing.
   */
  protected function re_group(&$files, $components, $relative_uri, $file_info) {
    $first_component = array_shift($components);
    $count_components = count($components);

    if ($first_component === NULL || $first_component == '.') {
      $files['.'][$file_info->name] = $file_info;
    }
    else {
      if (!isset($files[$first_component])) {
        $files[$first_component] = array(
          '.' => array(),
        );
      }
      $this->re_group($files[$first_component], $components, $relative_uri, $file_info);
    }
  }

  public function preprocess() {
    $added = parent::preprocess();

    foreach ($added as $object) {
      if ($object instanceof IslandoraBookBookBatchObject && !($object instanceof IslandoraBookPageBatchObject)) {
        $children = db_select('islandora_batch_queue', 'q')
          ->fields('q', array('id'))
          ->condition('parent', $object->id);

        // Set pages ready to go.
        db_update('islandora_batch_state')
          ->fields(array(
            'state' => 1,
          ))
          ->condition('id', $children, 'IN')
          ->execute();
        // Set book ready to go (after children, that is).
        db_update('islandora_batch_state')
          ->fields(array(
            'state' => 2,
          ))
          ->condition('id', $object->id)
          ->execute();
      }
    }

    return $added;
  }
}

class IslandoraBookBookBatchObject extends IslandoraScanBatchObject {
  public function initial_batch_state() {
    // Not ready until we've added the pages.
    return 0;
  }

  public function get_children(IslandoraTuque $connection) {
    // Create page objects, return in an array.
    $children = array();

    foreach ($this->object_info as $sequence => $info) {
      if ($sequence != '.') {
        $children[] = new IslandoraBookPageBatchObject($connection, $this->id, $sequence, $info);
      }
    }

    return $children;
  }

  public function batch_process() {
    // Use object_info to create some datastreams.
    $this->add_relationships();
    $this->label = $this->get_title();
    $this->get_mods();
    $this->get_dc();

    // Add other assets...
    // Skip MODS and DC, 'cause they get handled in other functions.
    $skip = array('MODS', 'DC');
    $skip = array_map(NULL, $skip);

    foreach (array_diff_key($this->object_info['.'], $skip) as $dsid => $obj) {
      $obj_datastream = $this->constructDatastream($dsid);
      // Figure out the mimetype.
      $obj_datastream->mimetype = static::get_mime($obj);
      $obj_datastream->label = "$dsid Datastream";
      $obj_datastream->setContentFromFile($obj->uri, FALSE);

      $this->ingestDatastream($obj_datastream);
    }

    // XXX: Should probably use constants...
    return 3;
  }

  protected function add_relationships() {
    // Add relationship to collection.
    $this->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', 'islandora:bookCollection');
    // Add content model relationship.
    $this->models = 'islandora:bookCModel';
  }

  protected function get_mods() {
    if (!isset($this['MODS'])) {
      $mods_datastream = $this->constructDatastream('MODS', 'X');
      $mods_datastream->mimetype = 'text/xml';
      $mods_datastream->label = 'MODS Record';

      $dir = drupal_get_path('module', 'islandora_batch');

      if (isset($this->object_info['.']['MODS'])) {
        $mods_datastream->setContentFromFile($this->object_info['.']['MODS']->uri, FALSE);
      }
      elseif (isset($this->object_info['.']['DC'])) {
        $mods_datastream->content = static::run_xsl_transform(array(
          'input' => file_get_contents($this->object_info['.']['DC']->uri),
          'xsl' => $dir . '/transforms/simpleDC2MODS.xsl',
        ));
      }
      elseif (isset($this->object_info['.']['--METADATA--'])) {
        // TODO: Detect type of contents, and transform.
      }

      if (!isset($mods_datastream->content)) {
      // If we do not yet have any XML, create a dumb MODS with just the
      //   title, and set it...
        $title = $this->base_name;

        $mods_datastream->content = <<<EOXML
<mods xmlns:mods="http://www.loc.gov/mods/v3" xmlns="http://www.loc.gov/mods/v3">
  <titleInfo>
    <title>$title</title>
  </titleInfo>
</mods>
EOXML;
      }

      $this->ingestDatastream($mods_datastream);
    }

    return $this['MODS']->content;
  }

  protected function get_dc() {
    if (!isset($this['DC'])) {
      // Create the DC datastream on this object.
      $dc_datastream = $this->constructDatastream('DC', 'X');
      $dc_datastream->mimetype = 'text/xml';
      $dc_datastream->label = 'DC Record';

      if (isset($this->object_info['.']['DC'])) {
        $dc_datastream->setContentFromFile($this->object_info['xml']->uri, FALSE);
      }
      elseif (isset($this->object_info['.']['--METADATA--'])) {
        // TODO: If --METADATA-- contains DC directly, use it.
      }

      // Get the DC by transforming from MODS.
      if (!isset($dc_datastream->content)) {
        // XXX: Might want to make this use a file, instead of staging the DC
        //   in the database table (inside the object we serialize).
        $new_dc = static::run_xsl_transform(array(
          'xsl' => drupal_get_path('module', 'islandora_batch') . '/transforms/mods_to_dc.xsl',
          'input' => $this->get_mods(),
        ));
        if ($new_dc) {
          $dc_datastream->content = $new_dc;
        }
      }

      $this->ingestDatastream($dc_datastream);
    }

    return $this['DC']->content;
  }
}

class IslandoraBookPageBatchObject extends IslandoraBookBookBatchObject {
  protected $parent_id, $sequence_number;

  public function __construct(IslandoraTuque $connection, $parent_id, $sequence, $object_info) {
    parent::__construct($connection, "$parent_id-$sequence", $object_info);

    $this->parent_id = $parent_id;
    $this->sequence_number = $sequence;
  }

  public function get_children(IslandoraTuque $connection) {
    // Pages should not have children.
    return array();
  }

  public function initial_batch_state() {
    // Not ready.
    return 0;
  }

  protected function add_relationships() {
    module_load_include('inc', 'islandora_book', 'includes/utilities');
    // Add relationship to collection.
    $rels_ext = $this->relationships;
    islandora_book_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isPageOf', $this->parent_id);
    islandora_book_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isSequenceNumber', (string) $this->sequence_number, TRUE);
    islandora_book_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isPageNumber', (string) $this->sequence_number, TRUE);
    islandora_book_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isSection', '1', TRUE);
    islandora_book_set_relationship($rels_ext, FEDORA_RELS_EXT_URI, 'isMemberOf', $this->parent_id);
    // Add content model relationship.
    $this->models = 'islandora:pageCModel';
  }
}

